import argparse
import csv
import hashlib
import io
import json
import os
import stat
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4


PROJECT_ROOT = Path(r"E:\Games\Adv32roid")
DEFAULT_CSV = Path(
    r"C:\Users\adamb\AppData\Local\Temp\adv32roid-storage-recheck-20261109-085914"
) / "qa-library-cleanup-reviewed.csv"
CSV_SHA256 = "78a0110939c06edcefc3ac4dec97f0227dca9ab2170c48d44bf1f8e6551fa500"
FINGERPRINTS_SHA256 = "000c892bf44276dfbfbd760adac03bc68e761eff7134e09c72d2be9f7b7844ed"
EXPECTED_FILES = 616
EXPECTED_BYTES = 814652920
LIBRARIES = {
    "libarisa32.so", "libavcodec.so", "libavformat.so", "libavutil.so",
    "libc++_shared.so", "libswresample.so", "libswscale.so",
}
QA_GROUPS = {
    "dc2dm-check", "dc2dm-save-validation", "dc4-replay-validation",
    "sakura-validation",
}
RETAINED_DIRECTORIES = {
    "playback-5c24a5223090", "jni-probe-d567223a2b43", "2f538de2b833",
    "jni-probe-4455c363cbeb", "jni-probe-c41601d1c4ee", "jni-probe-c02d1ed3d29c",
}


class SafetyError(Exception):
    pass


@dataclass(frozen=True)
class Entry:
    path: Path
    size: int
    mtime_ns: int
    sha256: str


@dataclass(frozen=True)
class Profile:
    csv_path: Path
    csv_sha256: str
    fingerprints_name: str
    fingerprints_sha256: str
    expected_files: int
    expected_bytes: int
    allowed_roots: tuple
    allowed_names: frozenset
    allowed_suffixes: frozenset
    log_prefix: str


def library_profile():
    return Profile(
        DEFAULT_CSV, CSV_SHA256, "qa-library-cleanup-fingerprints.json",
        FINGERPRINTS_SHA256, EXPECTED_FILES, EXPECTED_BYTES,
        tuple(Path("build") / group for group in QA_GROUPS),
        frozenset(LIBRARIES), frozenset(), "qa-library-cleanup",
    )


def trusted_bytes(path, expected_hash):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected_hash:
        raise SafetyError(f"Daftar/fingerprint berbeda dari hasil tinjauan: {path}")
    return data


def validate_scope(path, profile=None):
    profile = profile or library_profile()
    if not path.is_absolute() or ".." in path.parts:
        raise SafetyError(f"Path harus absolut tanpa '..': {path}")
    if any(":" in part for part in path.parts[1:]):
        raise SafetyError(f"Path stream alternatif tidak diizinkan: {path}")
    try:
        relative = path.relative_to(PROJECT_ROOT)
    except ValueError as exc:
        raise SafetyError(f"Path di luar proyek: {path}") from exc
    if (
        not any(relative.is_relative_to(root) for root in profile.allowed_roots)
        or (profile.allowed_names and path.name not in profile.allowed_names)
        or (profile.allowed_suffixes and path.suffix.lower() not in profile.allowed_suffixes)
        or path.name.lower() == "preview.bmp"
        or path.name.lower().endswith(".preview.bmp")
        or any(part.lower() in RETAINED_DIRECTORIES for part in relative.parts)
    ):
        raise SafetyError(f"Path bukan kandidat pembersihan yang diizinkan: {path}")


def load_plan(csv_path, profile=None):
    profile = profile or library_profile()
    data = trusted_bytes(csv_path, profile.csv_sha256)
    fingerprints_path = csv_path.with_name(profile.fingerprints_name)
    fingerprints = json.loads(trusted_bytes(fingerprints_path, profile.fingerprints_sha256))
    if fingerprints["csv_sha256"] != profile.csv_sha256:
        raise SafetyError("Fingerprint tidak cocok dengan CSV.")
    reader = csv.DictReader(io.StringIO(data.decode("utf-8-sig")))
    if reader.fieldnames != ["path", "bytes", "reason"]:
        raise SafetyError("Kolom CSV tidak sesuai.")
    entries = []
    seen = set()
    for row in reader:
        path = Path(row["path"])
        validate_scope(path, profile)
        key = os.path.normcase(str(path))
        if key in seen:
            raise SafetyError(f"Path duplikat: {path}")
        seen.add(key)
        record = fingerprints["files"][str(path)]
        size = int(row["bytes"])
        if size < 0 or size != record["bytes"]:
            raise SafetyError(f"Ukuran CSV/fingerprint tidak sesuai: {path}")
        entries.append(Entry(path, size, record["mtime_ns"], record["sha256"]))
    if (
        len(entries) != profile.expected_files
        or sum(entry.size for entry in entries) != profile.expected_bytes
        or {str(entry.path) for entry in entries} != set(fingerprints["files"])
    ):
        raise SafetyError("Jumlah, ukuran, atau anggota daftar berubah.")
    return entries


def checked_stat(path, profile=None):
    validate_scope(path, profile)
    for component in reversed((path, *path.parents)):
        try:
            info = component.lstat()
        except FileNotFoundError:
            return None
        if stat.S_ISLNK(info.st_mode) or (
            getattr(info, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT
        ):
            raise SafetyError(f"Symlink/junction/reparse point ditolak: {component}")
        if component != path and not stat.S_ISDIR(info.st_mode):
            raise SafetyError(f"Induk path bukan direktori: {component}")
    if not stat.S_ISREG(info.st_mode):
        raise SafetyError(f"Target bukan file biasa: {path}")
    return info


def identity(info):
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def inspect_entry(entry, expected_identity=None, profile=None):
    before = checked_stat(entry.path, profile)
    if before is None:
        return None
    stamp = identity(before)
    if before.st_size != entry.size or before.st_mtime_ns != entry.mtime_ns:
        raise SafetyError(f"Ukuran/waktu modifikasi berubah sejak audit: {entry.path}")
    if expected_identity is not None and stamp != expected_identity:
        raise SafetyError(f"Identitas file berubah setelah preflight: {entry.path}")
    with entry.path.open("rb") as stream:
        if identity(os.fstat(stream.fileno())) != stamp:
            raise SafetyError(f"File diganti ketika dibuka: {entry.path}")
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if identity(os.fstat(stream.fileno())) != stamp:
            raise SafetyError(f"File berubah ketika dibaca: {entry.path}")
    after = checked_stat(entry.path, profile)
    if after is None or identity(after) != stamp or digest != entry.sha256:
        raise SafetyError(f"Isi/identitas file tidak cocok dengan audit: {entry.path}")
    return stamp


def tracked_paths():
    try:
        result = subprocess.run(
            ["git", "-C", str(PROJECT_ROOT), "ls-files", "-z", "--", "build"],
            check=True, capture_output=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise SafetyError("Tidak dapat memverifikasi file yang dilacak Git.") from exc
    return {
        os.path.normcase(str(PROJECT_ROOT / os.fsdecode(name)))
        for name in result.stdout.split(b"\0") if name
    }


def preflight(entries, profile=None):
    tracked = tracked_paths()
    ready = []
    missing = 0
    for entry in entries:
        if os.path.normcase(str(entry.path)) in tracked:
            raise SafetyError(f"File kini dilacak Git; pembersihan ditolak: {entry.path}")
        stamp = inspect_entry(entry, profile=profile)
        if stamp is None:
            missing += 1
        else:
            ready.append((entry, stamp))
    return ready, missing


def delete_plan(ready, log_path, profile=None):
    removed = 0
    removed_bytes = 0
    with log_path.open("x", encoding="utf-8") as log:
        def record(event, **values):
            log.write(json.dumps({"event": event, **values}, ensure_ascii=False) + "\n")
            log.flush()

        record("start", time=datetime.now(timezone.utc).isoformat(), files=len(ready))
        try:
            for entry, stamp in ready:
                current = inspect_entry(entry, stamp, profile)
                if current is None:
                    record("already_missing", path=str(entry.path))
                    continue
                final = checked_stat(entry.path, profile)
                if final is None or identity(final) != current:
                    raise SafetyError(f"File berubah tepat sebelum penghapusan: {entry.path}")
                entry.path.unlink()
                removed += 1
                removed_bytes += entry.size
                record("deleted", path=str(entry.path), bytes=entry.size)
        except (OSError, SafetyError, KeyboardInterrupt) as exc:
            record("stopped", error=str(exc), removed_files=removed, removed_bytes=removed_bytes)
            raise
        finally:
            print(f"Terhapus: {removed} file, {removed_bytes / 1048576:.2f} MiB.")
            print(f"Log: {log_path}")
        record("complete", removed_files=removed, removed_bytes=removed_bytes)


def main(argv=None, profile=None):
    profile = profile or library_profile()
    parser = argparse.ArgumentParser(
        description=f"Bersihkan hanya {profile.expected_files} file QA hasil tinjauan. Default: simulasi.",
        epilog="Jalankan saat build/probe berhenti. --delete menghapus permanen, bukan ke Recycle Bin.",
    )
    parser.add_argument("--csv", type=Path, default=profile.csv_path,
                        help="CSV reviewed asli; fingerprint harus berada di folder yang sama.")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--delete", action="store_true", help="Hapus permanen setelah seluruh validasi lulus.")
    mode.add_argument("--dry-run", action="store_true", help="Simulasi tanpa menghapus (default).")
    parser.add_argument("--verbose", action="store_true", help="Tampilkan seluruh path yang lolos validasi.")
    args = parser.parse_args(argv)
    if os.name != "nt":
        parser.error("Skrip ini khusus Windows.")
    try:
        if not PROJECT_ROOT.is_dir():
            raise SafetyError(f"Folder proyek tidak ditemukan: {PROJECT_ROOT}")
        entries = load_plan(args.csv, profile)
        print(f"Memvalidasi {len(entries)} file, termasuk SHA256...", flush=True)
        ready, missing = preflight(entries, profile)
        total = sum(entry.size for entry, _ in ready)
        print(f"Lolos validasi: {len(ready)} file, {total / 1048576:.2f} MiB.")
        print(f"Sudah tidak ada: {missing} file. Target hanya file dalam daftar hasil tinjauan.")
        if args.verbose:
            for entry, _ in ready:
                print(entry.path)
        if not args.delete:
            print("SIMULASI: tidak ada file dihapus. Gunakan --delete untuk menghapus permanen.")
            return 0
        if not ready:
            print("Tidak ada kandidat tersisa untuk dihapus.")
            return 0
        log_path = args.csv.parent / (
            profile.log_prefix + "-" + datetime.now().strftime("%Y%m%d-%H%M%S")
            + "-" + uuid4().hex[:8] + ".jsonl"
        )
        print("Menghapus permanen file yang lolos validasi...", flush=True)
        delete_plan(ready, log_path, profile)
        return 0
    except (OSError, SafetyError, ValueError, KeyError) as exc:
        print(f"DIHENTIKAN: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("DIHENTIKAN oleh pengguna.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())

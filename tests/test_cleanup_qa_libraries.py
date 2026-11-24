import contextlib
import csv
from dataclasses import replace
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "cleanup_qa_libraries.py"
SPEC = importlib.util.spec_from_file_location("cleanup_qa_libraries", SCRIPT)
cleanup = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = cleanup
SPEC.loader.exec_module(cleanup)


class CleanupSafetyTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="adv32roid-cleanup-test-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.root = self.directory / "Adv32roid"
        self.root.mkdir()
        self.csv_path = self.directory / "qa-library-cleanup-reviewed.csv"
        self.fingerprints_path = self.directory / "qa-library-cleanup-fingerprints.json"
        self.start_patch(mock.patch.object(cleanup, "PROJECT_ROOT", self.root))
        self.start_patch(mock.patch.object(cleanup, "DEFAULT_CSV", self.csv_path))
        self.start_patch(mock.patch.object(cleanup, "tracked_paths", return_value=set()))
        self.start_patch(mock.patch.object(
            cleanup, "os", SimpleNamespace(name="nt", path=os.path, fstat=os.fstat)
        ))

    def start_patch(self, patcher):
        result = patcher.start()
        self.addCleanup(patcher.stop)
        return result

    def create_entry(self, run="fixture", content=b"original", filename="libarisa32.so"):
        path = self.root / "build" / "dc4-replay-validation" / run / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        info = path.stat()
        return cleanup.Entry(path, len(content), info.st_mtime_ns,
                             hashlib.sha256(content).hexdigest())

    def write_plan(self, entries):
        with self.csv_path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=["path", "bytes", "reason"])
            writer.writeheader()
            for entry in entries:
                writer.writerow({"path": str(entry.path), "bytes": entry.size,
                                 "reason": "Temporary test fixture"})
        csv_hash = hashlib.sha256(self.csv_path.read_bytes()).hexdigest()
        fingerprints = {
            "csv_sha256": csv_hash,
            "files": {
                str(entry.path): {"bytes": entry.size, "mtime_ns": entry.mtime_ns,
                                  "sha256": entry.sha256}
                for entry in entries
            },
        }
        self.fingerprints_path.write_text(json.dumps(fingerprints), encoding="utf-8")
        fingerprint_hash = hashlib.sha256(self.fingerprints_path.read_bytes()).hexdigest()
        self.start_patch(mock.patch.multiple(
            cleanup, CSV_SHA256=csv_hash, FINGERPRINTS_SHA256=fingerprint_hash,
            EXPECTED_FILES=len(entries), EXPECTED_BYTES=sum(entry.size for entry in entries),
        ))

    def run_main(self, *arguments, profile=None):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            result = cleanup.main(list(arguments), profile=profile)
        return result, stdout.getvalue(), stderr.getvalue()

    def image_profile(self):
        return replace(
            cleanup.library_profile(),
            allowed_roots=(Path("build") / "dc4-replay-validation",),
            allowed_names=frozenset(), allowed_suffixes=frozenset({".png", ".bmp"}),
            log_prefix="dc4-analysis-cleanup",
        )

    def test_image_profile_dry_run_keeps_library_scope_separate(self):
        entry = self.create_entry(filename="capture.png")
        self.write_plan([entry])
        profile = self.image_profile()
        with self.assertRaises(cleanup.SafetyError):
            cleanup.validate_scope(entry.path)
        with mock.patch.object(Path, "unlink", autospec=True) as unlink:
            result, stdout, stderr = self.run_main(profile=profile)
        self.assertEqual(result, 0, stderr)
        self.assertIn("SIMULASI", stdout)
        unlink.assert_not_called()
        for path in (
            self.root / "build" / "dc2dm-check" / "capture.png",
            self.root / "scratch" / "capture.png",
            entry.path.with_name("analysis.py"),
        ):
            with self.subTest(path=path):
                with self.assertRaises(cleanup.SafetyError):
                    cleanup.validate_scope(path, profile)

    def test_save_thumbnail_is_rejected_even_in_pinned_image_manifest(self):
        for filename in ("slot01.preview.bmp", "preview.bmp"):
            with self.subTest(filename=filename):
                entry = self.create_entry(filename=filename)
                self.write_plan([entry])
                with mock.patch.object(Path, "unlink", autospec=True) as unlink:
                    result, _, stderr = self.run_main("--delete", profile=self.image_profile())
                self.assertEqual(result, 1)
                self.assertIn("bukan kandidat", stderr)
                unlink.assert_not_called()

    def test_invalid_last_image_prevents_all_deletions(self):
        entries = [self.create_entry(name, filename="capture.png")
                   for name in ("first", "second", "last")]
        self.write_plan(entries)
        entries[-1].path.write_bytes(b"new capture after audit")
        with mock.patch.object(Path, "unlink", autospec=True) as unlink:
            result, _, stderr = self.run_main("--delete", profile=self.image_profile())
        self.assertEqual(result, 1)
        self.assertIn("berubah sejak audit", stderr)
        unlink.assert_not_called()

    def test_default_is_dry_run_without_unlink_or_deletion_log(self):
        entries = [self.create_entry("first"), self.create_entry("second")]
        self.write_plan(entries)
        with mock.patch.object(Path, "unlink", autospec=True) as unlink:
            result, stdout, stderr = self.run_main()
        self.assertEqual(result, 0, stderr)
        unlink.assert_not_called()
        self.assertIn("SIMULASI", stdout)
        self.assertEqual(list(self.directory.glob("*.jsonl")), [])

    def test_pinned_empty_file_is_validated_before_mocked_deletion(self):
        entry = self.create_entry(content=b"", filename="py.typed")
        self.write_plan([entry])
        profile = replace(self.image_profile(), allowed_suffixes=frozenset({".typed"}))
        with mock.patch.object(Path, "unlink", autospec=True) as unlink:
            result, stdout, stderr = self.run_main(profile=profile)
            self.assertEqual(result, 0, stderr)
            self.assertIn("1 file, 0.00 MiB", stdout)
            unlink.assert_not_called()
            result, _, stderr = self.run_main("--delete", profile=profile)
            self.assertEqual(result, 0, stderr)
            unlink.assert_called_once_with(entry.path)
        self.assertEqual(entry.path.read_bytes(), b"")

    def test_newly_tracked_candidate_prevents_all_deletions(self):
        entries = [self.create_entry("first"), self.create_entry("last")]
        self.write_plan(entries)
        with mock.patch.object(cleanup, "tracked_paths", return_value={
            os.path.normcase(str(entries[-1].path)),
        }), mock.patch.object(Path, "unlink", autospec=True) as unlink:
            result, _, stderr = self.run_main("--delete")
        self.assertEqual(result, 1)
        self.assertIn("dilacak Git", stderr)
        unlink.assert_not_called()
        self.assertTrue(all(entry.path.read_bytes() == b"original" for entry in entries))

    def test_modified_csv_is_rejected_before_preflight_or_deletion(self):
        self.write_plan([self.create_entry()])
        with self.csv_path.open("ab") as stream:
            stream.write(b"\n")
        with mock.patch.object(Path, "unlink", autospec=True) as unlink, \
                mock.patch.object(cleanup, "preflight") as preflight:
            result, _, stderr = self.run_main("--delete")
        self.assertEqual(result, 1)
        self.assertIn("Daftar/fingerprint berbeda", stderr)
        preflight.assert_not_called()
        unlink.assert_not_called()

    def test_modified_fingerprint_is_rejected(self):
        self.write_plan([self.create_entry()])
        with self.fingerprints_path.open("ab") as stream:
            stream.write(b"\n")
        with self.assertRaises(cleanup.SafetyError):
            cleanup.load_plan(self.csv_path)

    def test_duplicate_plan_paths_are_rejected(self):
        entry = self.create_entry()
        self.write_plan([entry, entry])
        with self.assertRaisesRegex(cleanup.SafetyError, "duplikat"):
            cleanup.load_plan(self.csv_path)

    def test_invalid_last_target_prevents_every_deletion(self):
        entries = [self.create_entry(name) for name in ("first", "second", "last")]
        self.write_plan(entries)
        entries[-1].path.write_bytes(b"changed after review")
        with mock.patch.object(Path, "unlink", autospec=True) as unlink:
            result, _, stderr = self.run_main("--delete")
        self.assertEqual(result, 1)
        self.assertIn("berubah sejak audit", stderr)
        unlink.assert_not_called()
        self.assertTrue(all(entry.path.is_file() for entry in entries))
        self.assertEqual(list(self.directory.glob("*.jsonl")), [])

    def test_missing_target_is_skipped(self):
        entry = self.create_entry()
        missing = cleanup.Entry(entry.path.parent / "libavcodec.so", 8, 0, "0" * 64)
        ready, missing_count = cleanup.preflight([entry, missing])
        self.assertEqual([item[0] for item in ready], [entry])
        self.assertEqual(missing_count, 1)

    def test_permission_error_is_not_treated_as_missing(self):
        entry = self.create_entry()
        original_lstat = Path.lstat

        def lstat(path):
            if path == entry.path:
                raise PermissionError("Cannot inspect fixture")
            return original_lstat(path)

        with mock.patch.object(Path, "lstat", autospec=True, side_effect=lstat):
            with self.assertRaises(PermissionError):
                cleanup.preflight([entry])

    def test_directory_with_library_name_is_rejected(self):
        target = self.root / "build" / "dc4-replay-validation" / "fixture" / "libarisa32.so"
        target.mkdir(parents=True)
        with self.assertRaisesRegex(cleanup.SafetyError, "bukan file biasa"):
            cleanup.checked_stat(target)

    def test_file_and_ancestor_reparse_points_are_rejected(self):
        entry = self.create_entry()
        original_lstat = Path.lstat
        for component in (entry.path, entry.path.parent, self.root):
            with self.subTest(component=component):
                def lstat(path):
                    info = original_lstat(path)
                    if path == component:
                        return SimpleNamespace(
                            st_mode=info.st_mode,
                            st_file_attributes=stat.FILE_ATTRIBUTE_REPARSE_POINT,
                        )
                    return info

                with mock.patch.object(Path, "lstat", autospec=True, side_effect=lstat):
                    with self.assertRaisesRegex(cleanup.SafetyError, "reparse point"):
                        cleanup.checked_stat(entry.path)

    def test_dangling_symlink_is_rejected_instead_of_skipped(self):
        entry = self.create_entry()
        missing = entry.path.parent / "libavcodec.so"
        original_lstat = Path.lstat

        def lstat(path):
            if path == missing:
                return SimpleNamespace(st_mode=stat.S_IFLNK, st_file_attributes=0)
            return original_lstat(path)

        with mock.patch.object(Path, "lstat", autospec=True, side_effect=lstat):
            with self.assertRaisesRegex(cleanup.SafetyError, "Symlink"):
                cleanup.checked_stat(missing)

    def test_same_size_content_change_with_restored_mtime_fails_hash(self):
        entry = self.create_entry(content=b"original")
        before = entry.path.stat()
        entry.path.write_bytes(b"modified")
        os.utime(entry.path, ns=(before.st_atime_ns, entry.mtime_ns))
        after = entry.path.stat()
        self.assertEqual(after.st_size, entry.size)
        self.assertEqual(after.st_mtime_ns, entry.mtime_ns)
        with self.assertRaisesRegex(cleanup.SafetyError, "Isi/identitas"):
            cleanup.inspect_entry(entry)

    def test_file_replacement_after_preflight_is_rejected(self):
        entry = self.create_entry()
        ready, _ = cleanup.preflight([entry])
        replacement = entry.path.with_name("replacement.tmp")
        replacement.write_bytes(b"original")
        os.utime(replacement, ns=(entry.path.stat().st_atime_ns, entry.mtime_ns))
        os.replace(replacement, entry.path)
        with self.assertRaisesRegex(cleanup.SafetyError, "Identitas file berubah"):
            cleanup.inspect_entry(entry, ready[0][1])

    def test_scope_rejects_dependencies_outside_root_and_retained_sets(self):
        retained = (
            "playback-5c24a5223090", "jni-probe-d567223a2b43", "2f538de2b833",
            "jni-probe-4455c363cbeb", "jni-probe-c41601d1c4ee", "jni-probe-c02d1ed3d29c",
        )
        paths = [
            self.root / "third_party" / "ffmpeg" / "libarisa32.so",
            self.directory / "outside" / "libarisa32.so",
            self.root / "build" / "dc4-replay-validation" / "fixture" / "notes.txt",
        ]
        paths += [self.root / "build" / "dc4-replay-validation" / name / "libarisa32.so"
                  for name in retained]
        for path in paths:
            with self.subTest(path=path):
                with self.assertRaises(cleanup.SafetyError):
                    cleanup.validate_scope(path)

    def test_delete_error_stops_and_logs_partial_count(self):
        entries = [self.create_entry(name) for name in ("first", "locked", "third")]
        ready, _ = cleanup.preflight(entries)
        log_path = self.directory / "partial.jsonl"
        stdout = io.StringIO()
        with mock.patch.object(Path, "unlink", autospec=True,
                               side_effect=[None, PermissionError("Fixture locked")]) as unlink, \
                contextlib.redirect_stdout(stdout):
            with self.assertRaises(PermissionError):
                cleanup.delete_plan(ready, log_path)
        self.assertEqual(unlink.call_args_list, [mock.call(entries[0].path),
                                                mock.call(entries[1].path)])
        events = [json.loads(line) for line in log_path.read_text(encoding="utf-8").splitlines()]
        self.assertEqual([event["event"] for event in events], ["start", "deleted", "stopped"])
        self.assertEqual(events[-1]["removed_files"], 1)
        self.assertEqual(events[-1]["removed_bytes"], entries[0].size)
        self.assertIn("Fixture locked", events[-1]["error"])
        self.assertIn("Terhapus: 1 file", stdout.getvalue())
        self.assertTrue(all(entry.path.is_file() for entry in entries))

    def test_delete_rechecks_changed_file_before_unlink(self):
        entry = self.create_entry()
        ready, _ = cleanup.preflight([entry])
        entry.path.write_bytes(b"changed")
        log_path = self.directory / "changed.jsonl"
        with mock.patch.object(Path, "unlink", autospec=True) as unlink, \
                contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(cleanup.SafetyError):
                cleanup.delete_plan(ready, log_path)
        unlink.assert_not_called()
        events = [json.loads(line) for line in log_path.read_text(encoding="utf-8").splitlines()]
        self.assertEqual(events[-1]["event"], "stopped")
        self.assertEqual(events[-1]["removed_files"], 0)


if __name__ == "__main__":
    unittest.main()

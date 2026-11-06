"""Run message portrait regressions against the arm64 library packaged in an APK."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import uuid
import zipfile


def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", type=Path, default=repo / "android/app/build/outputs/apk/release/app-release.apk")
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("ANDROID_HOME", str(Path.home() / "AppData/Local/Android/Sdk"))))
    parser.add_argument("--serial")
    args = parser.parse_args()
    run_id = uuid.uuid4().hex[:12]
    output = repo / "build/dc4-face-validation" / ("android-" + run_id)
    output.mkdir(parents=True)
    host = "windows-x86_64" if os.name == "nt" else "linux-x86_64"
    compiler = args.sdk / "ndk/27.3.13750724/toolchains/llvm/prebuilt" / host / "bin" / (
        "aarch64-linux-android26-clang++.cmd" if os.name == "nt" else "aarch64-linux-android26-clang++")
    adb = [str(args.sdk / "platform-tools" / ("adb.exe" if os.name == "nt" else "adb"))]
    if args.serial:
        adb += ["-s", args.serial]
    with zipfile.ZipFile(args.apk) as apk:
        for name in apk.namelist():
            if name.startswith("lib/arm64-v8a/") and name.endswith(".so"):
                (output / Path(name).name).write_bytes(apk.read(name))
    binary = output / "dc4_message_face_test"
    subprocess.run([str(compiler), "-std=c++17", "-O2", "-I" + str(repo / "core/include"),
                    str(repo / "tests/dc4_message_face_test.cpp"), "-L" + str(output),
                    "-larisa32", "-lc++_shared", "-Wl,-z,max-page-size=16384", "-o", str(binary)], check=True)
    library_hash = hashlib.sha256((output / "libarisa32.so").read_bytes()).hexdigest()
    device = "/data/local/tmp/arisa-dc4-faces-" + run_id
    print("Evidence:", output, "\nLibrary SHA256:", library_hash, flush=True)
    subprocess.run(adb + ["shell", "mkdir", "-p", device], check=True)
    for path in [binary, *sorted(output.glob("*.so"))]:
        subprocess.run(adb + ["push", str(path), device + "/" + path.name], check=True, capture_output=True)
    subprocess.run(adb + ["shell", "chmod", "700", device + "/dc4_message_face_test"], check=True)
    result = subprocess.run(adb + ["shell", "env", "LD_LIBRARY_PATH=" + device,
                                   "TMPDIR=" + device, device + "/dc4_message_face_test"],
                            capture_output=True, text=True, timeout=180)
    log = "libarisa32.so SHA256=" + library_hash + "\n" + result.stdout + result.stderr
    (output / "result.log").write_text(log, encoding="utf-8")
    print(log, flush=True)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()

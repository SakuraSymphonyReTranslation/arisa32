"""Run the DC4 effect tests against the APK's arm64 library without opening the app."""
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
    parser.add_argument("--apk", type=Path, default=repo / "android/app/build/outputs/apk/debug/app-debug.apk")
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("ANDROID_HOME", str(Path.home() / "AppData/Local/Android/Sdk"))))
    parser.add_argument("--ndk", default="27.3.13750724")
    parser.add_argument("--dc4-data", type=Path, help="Optional local AdvData for original-asset rendering")
    parser.add_argument("--serial", help="ADB device serial")
    args = parser.parse_args()
    run_id = uuid.uuid4().hex[:12]
    output = repo / "build/sakura-validation" / ("android-" + run_id)
    output.mkdir(parents=True)
    host = "windows-x86_64" if os.name == "nt" else "linux-x86_64"
    compiler = args.sdk / "ndk" / args.ndk / "toolchains/llvm/prebuilt" / host / "bin" / (
        "aarch64-linux-android26-clang++.cmd" if os.name == "nt" else "aarch64-linux-android26-clang++")
    adb = [str(args.sdk / "platform-tools" / ("adb.exe" if os.name == "nt" else "adb"))]
    if args.serial:
        adb += ["-s", args.serial]
    with zipfile.ZipFile(args.apk) as apk:
        for name in apk.namelist():
            if name.startswith("lib/arm64-v8a/") and name.endswith(".so"):
                (output / Path(name).name).write_bytes(apk.read(name))
    binary = output / "dc4_effect_test"
    subprocess.run([str(compiler), "-std=c++17", "-O2", "-I" + str(repo / "core/include"),
                    str(repo / "tests/dc4_effect_test.cpp"), "-L" + str(output),
                    "-larisa32", "-lc++_shared", "-Wl,-z,max-page-size=16384", "-o", str(binary)], check=True)
    library_hash = hashlib.sha256((output / "libarisa32.so").read_bytes()).hexdigest()
    device = "/data/local/tmp/arisa-dc4-effects-" + run_id
    print("Evidence:", output, "\nLibrary SHA256:", library_hash, flush=True)
    subprocess.run(adb + ["shell", "mkdir", "-p", device], check=True)
    for path in [binary, *sorted(output.glob("*.so"))]:
        subprocess.run(adb + ["push", str(path), device + "/" + path.name], check=True, capture_output=True)
    env = ["LD_LIBRARY_PATH=" + device]
    if args.dc4_data:
        for name in ["SYSTEM/EFFCHANGESCREEN1.CRX", "SYSTEM/EFFSAKURA01.CRX", "SYSTEM/EFFYOUSEI.CRX", "MSK/MASK15B.CRX"]:
            destination = device + "/original/GRP/" + name
            subprocess.run(adb + ["shell", "mkdir", "-p", destination.rsplit("/", 1)[0]], check=True)
            subprocess.run(adb + ["push", str(args.dc4_data / "GRP" / name), destination], check=True, capture_output=True)
        env.append("DC4_DATA=" + device + "/original")
    subprocess.run(adb + ["shell", "chmod", "700", device + "/dc4_effect_test"], check=True)
    result = subprocess.run(adb + ["shell", "env", *env, device + "/dc4_effect_test", device + "/fixtures"],
                            capture_output=True, text=True, timeout=180)
    log = "libarisa32.so SHA256=" + library_hash + "\n" + result.stdout + result.stderr
    (output / "result.log").write_text(log, encoding="utf-8")
    print(log, flush=True)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()

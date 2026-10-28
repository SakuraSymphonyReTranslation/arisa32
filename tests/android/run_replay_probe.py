"""Run production JNI replay checks in a private device temp directory (no UI/install)."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import uuid
import zipfile


def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", type=Path, default=repo / "android/app/build/outputs/apk/debug/app-debug.apk")
    parser.add_argument("--libraries", type=Path, help="Use previously extracted arm64 libraries instead of an APK")
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("ANDROID_HOME", str(Path.home() / "AppData/Local/Android/Sdk"))))
    parser.add_argument("--jdk", type=Path, default=Path(os.environ.get("JAVA_HOME", "C:/Program Files/Android/Android Studio/jbr")))
    args = parser.parse_args()
    run_id = uuid.uuid4().hex[:12]
    output = repo / "build/dc4-replay-validation" / ("jni-probe-" + run_id)
    output.mkdir(parents=True)
    classes = output / "classes"
    classes.mkdir()
    suffix = ".exe" if os.name == "nt" else ""
    java = args.jdk / "bin" / ("java" + suffix)
    javac = args.jdk / "bin" / ("javac" + suffix)
    adb = args.sdk / "platform-tools" / ("adb" + suffix)
    build_tools = sorted((args.sdk / "build-tools").glob("*/lib/d8.jar"))[-1]
    source = repo / "tests/android/replay-probe/org/dc4engine/Arisa32Activity.java"
    subprocess.run([str(javac), "--release", "8", "-d", str(classes), str(source)], check=True)
    subprocess.run([str(java), "-cp", str(build_tools), "com.android.tools.r8.D8", "--min-api", "26",
                    "--output", str(output), str(classes / "org/dc4engine/Arisa32Activity.class")], check=True)
    if args.libraries:
        for lib in args.libraries.glob("*.so"):
            shutil.copyfile(lib, output / lib.name)
    else:
        with zipfile.ZipFile(args.apk) as apk:
            for name in apk.namelist():
                if name.startswith("lib/arm64-v8a/") and name.endswith(".so"):
                    (output / Path(name).name).write_bytes(apk.read(name))
    library_hash = hashlib.sha256((output / "libarisa32.so").read_bytes()).hexdigest()
    device = "/data/local/tmp/arisa32-replay-probe-" + run_id
    print("Evidence:", output, "\nLibrary SHA256:", library_hash, "\nDevice fixture root:", device, flush=True)
    subprocess.run([str(adb), "shell", "mkdir", "-p", device], check=True)
    for path in [output / "classes.dex", *sorted(output.glob("*.so"))]:
        subprocess.run([str(adb), "push", str(path), device + "/" + path.name], check=True, capture_output=True)
    result = subprocess.run([str(adb), "shell", "env", "CLASSPATH=" + device + "/classes.dex",
                             "LD_LIBRARY_PATH=" + device, "app_process", "/data/local/tmp",
                             "org.dc4engine.Arisa32Activity", device], capture_output=True, text=True, timeout=120)
    log = "libarisa32.so SHA256=" + library_hash + "\n" + result.stdout + result.stderr
    (output / "result.log").write_text(log, encoding="utf-8")
    print(log, flush=True)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()

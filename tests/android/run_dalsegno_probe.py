"""Run Dal Segno's packaged JNI against read-only assets and isolated save copies."""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import uuid
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', required=True, type=Path)
    parser.add_argument('--saves', required=True, type=Path)
    parser.add_argument('--data', required=True, help='Read-only device AdvData directory')
    parser.add_argument('--font', type=Path, help='Explicit font override; omit to use the production game default')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--legacy-saves', type=Path, help='Copied Android profile for extras-legacy')
    parser.add_argument('--cases', nargs='+', choices=['name', 'name-restart', 'saves', 'backlog', 'toolbar', 'config', 'menus', 'transitions',
                                                       'extras-fresh', 'extras-imported', 'extras-legacy', 'render-timing', 'opening', 'wipes',
                                                       'petals', 'petals-two'],
                        default=['name', 'name-restart', 'saves', 'backlog'])
    parser.add_argument('--cpu-mask', type=lambda s: int(s, 16), help='Optional hexadecimal CPU affinity for this probe process')
    parser.add_argument('--abi', choices=['arm64-v8a', 'armeabi-v7a'], default='arm64-v8a')
    args = parser.parse_args()
    if 'name-restart' in args.cases and 'name' not in args.cases:
        parser.error('name-restart requires name earlier in the same run')
    if 'extras-legacy' in args.cases and args.legacy_saves is None:
        parser.error('extras-legacy requires --legacy-saves')
    sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'AppData/Local/Android/Sdk')))
    jdk = Path(os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr'))
    suffix = '.exe' if os.name == 'nt' else ''
    adb = [str(sdk / 'platform-tools' / ('adb' + suffix)), '-s', args.serial]
    out = Path(tempfile.mkdtemp(prefix='arisa32-dalsegno-jni-'))
    classes = out / 'classes'
    classes.mkdir()
    source = Path(__file__).resolve().parent / 'dalsegno-probe/org/dc4engine/Arisa32Activity.java'
    profiles = Path(__file__).resolve().parents[2] / 'android/app/src/main/java/org/dc4engine/profile'
    android_jar = sdk / 'platforms/android-36/android.jar'
    subprocess.run([str(jdk / 'bin' / ('javac' + suffix)), '--release', '8', '-encoding', 'UTF-8',
                    '-cp', str(android_jar), '-d', str(classes), str(source),
                    *map(str, profiles.glob('*.java'))], check=True)
    d8 = sorted((sdk / 'build-tools').glob('*/lib/d8.jar'))[-1]
    subprocess.run([str(jdk / 'bin' / ('java' + suffix)), '-cp', str(d8), 'com.android.tools.r8.D8',
                    '--min-api', '26', '--lib', str(android_jar), '--output', str(out),
                    *map(str, classes.rglob('*.class'))], check=True)
    with zipfile.ZipFile(args.apk) as apk:
        for path in apk.namelist():
            if path.startswith('lib/' + args.abi + '/') and path.endswith('.so'):
                (out / Path(path).name).write_bytes(apk.read(path))
    device = '/data/local/tmp/arisa32-dalsegno-' + uuid.uuid4().hex[:12]
    data = device + '/assets/AdvData'
    subprocess.run(adb + ['shell', shlex.join(['mkdir', '-p', data, device + '/input'])], check=True)
    # Only asset links are exposed; profile discovery cannot reach the player's SaveData.
    for name in ('GRP', 'MES', 'MOVIE', 'MUSIC', 'SE', 'VOICE.DAT'):
        subprocess.run(adb + ['shell', shlex.join(['ln', '-s', args.data.rstrip('/') + '/' + name,
                                                 data + '/' + name])], check=True)
    for file in [out / 'classes.dex', *out.glob('*.so')]:
        subprocess.run(adb + ['push', str(file), device + '/' + file.name], capture_output=True, check=True)
    if args.font:
        subprocess.run(adb + ['push', str(args.font), device + '/font.ttc'], capture_output=True, check=True)
    if any(not mode.startswith('extras-') for mode in args.cases):
        for slot in ('00', '01', '02', '03', '05', '06'):
            name = 'DSAO' + slot + '.dat'
            subprocess.run(adb + ['push', str(args.saves / name), device + '/input/' + name],
                           capture_output=True, check=True)
    for mode in ('extras-imported', 'extras-legacy'):
        if mode not in args.cases:
            continue
        target = device + '/profile-' + mode
        subprocess.run(adb + ['shell', shlex.join(['mkdir', '-p', target])], check=True)
        if mode == 'extras-imported':
            progress = bytearray((args.saves / 'system.dat').read_bytes())
            flag_offset = 0x2F0
            progress[flag_offset + 274:flag_offset + 415] = bytes(141)
            for cg in (274, 276, 284):
                progress[flag_offset + cg] = 1
            fixture = out / 'system.dat'
            fixture.write_bytes(progress)
            files = [fixture]
        else:
            files = [args.legacy_saves / name for name in ('system.dat', 'system-profile.dat', 'read-lines.txt')]
            files += sorted(args.legacy_saves.glob('DSAO[0-9][0-9].dat'))
        for file in files:
            subprocess.run(adb + ['push', str(file), target + '/' + file.name], capture_output=True, check=True)
    if 'transitions' in args.cases:
        checkpoint = bytearray((args.saves / 'DSAO01.dat').read_bytes())
        if len(checkpoint) != 0x11AF8 or int.from_bytes(checkpoint[0x83C:0x840], 'little') != 348:
            raise ValueError('transitions requires an automatic start checkpoint in DSAO01.dat')
        checkpoint[8:168] = b'DS_1201A_CM'.ljust(160, b'\0')
        fixture = out / 'date-start.dat'
        fixture.write_bytes(checkpoint)
        subprocess.run(adb + ['push', str(fixture), device + '/input/date-start.dat'],
                       capture_output=True, check=True)
    if 'petals-two' in args.cases:
        checkpoint = bytearray((args.saves / 'DSAO02.dat').read_bytes())
        if len(checkpoint) != 0x11AF8 or checkpoint[8:19].lower() != b'ds_1129b_cm':
            raise ValueError('petals-two requires a DS_1129B_CM manual save in DSAO02.dat')
        # Resume the copy at the last road background before the two-flower cue.
        checkpoint[8:168] = b'DS_1129C_CM'.ljust(160, b'\0')
        for i, value in enumerate((32502, 1102, 234, 32475)):
            checkpoint[0x83C+4*i:0x840+4*i] = value.to_bytes(4, 'little')
        fixture = out / 'petals-two.dat'
        fixture.write_bytes(checkpoint)
        subprocess.run(adb + ['push', str(fixture), device + '/input/' + fixture.name], capture_output=True, check=True)
    command = ['env',
               'CLASSPATH=' + device + '/classes.dex', 'LD_LIBRARY_PATH=' + device,
               'app_process64' if args.abi == 'arm64-v8a' else 'app_process32',
               '/data/local/tmp', 'org.dc4engine.Arisa32Activity', device,
               data, device + '/font.ttc' if args.font else '']
    log = ('APK SHA256=' + hashlib.sha256(args.apk.read_bytes()).hexdigest() + '\n'
           + 'JNI SHA256=' + hashlib.sha256((out / 'libarisa32.so').read_bytes()).hexdigest() + '\n'
           + 'Device fixture=' + device + '\n')
    status = 0
    print('Evidence:', out, flush=True)
    try:
        for mode in args.cases:
            try:
                limit = 300 if mode == 'petals' else 110
                invocation = ['toybox', 'timeout', '-k', '5s', f'{limit}s'] + command + [mode]
                if args.cpu_mask is not None:
                    invocation = ['toybox', 'taskset', format(args.cpu_mask, 'x')] + invocation
                result = subprocess.run(adb + ['shell', shlex.join(invocation)],
                                        capture_output=True, text=True, encoding='utf-8',
                                        errors='replace', timeout=limit + 10)
                output = result.stdout + result.stderr
                status = result.returncode
            except subprocess.TimeoutExpired:
                output, status = f'FAIL probe exceeded {limit + 10} seconds\n', 124
            log += '[' + mode + ']\n' + output
            (out / 'result.log').write_text(log, encoding='utf-8')
            print(mode + ': ' + output, flush=True)
            if status:
                break
        result = subprocess.run(adb + ['pull', device + '/captures', str(out)], capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=30)
        if result.returncode:
            log += 'Capture retrieval failed: ' + result.stdout + result.stderr
            status = status or result.returncode
    finally:
        # The UUID path is created here and never contains user-owned files.
        if device.startswith('/data/local/tmp/arisa32-dalsegno-'):
            subprocess.run(adb + ['shell', shlex.join(['rm', '-rf', device])], capture_output=True, timeout=15)
        (out / 'result.log').write_text(log, encoding='utf-8')
        print('Evidence:', out, flush=True)
    raise SystemExit(status)


if __name__ == '__main__':
    main()

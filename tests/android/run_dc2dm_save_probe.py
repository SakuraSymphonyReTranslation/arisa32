"""Verify DC2DM PC save copies with an APK's JNI library; game data stays read-only."""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import uuid
import zipfile


def process_output(result):
    return ''.join(part.decode('utf-8', errors='replace') if isinstance(part, bytes) else part or ''
                   for part in (result.stdout, result.stderr))


def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', required=True, type=Path)
    parser.add_argument('--saves', required=True, type=Path, help='Local copied PC SaveData directory')
    parser.add_argument('--data', required=True, help='Read-only device AdvData path')
    parser.add_argument('--exe', required=True, help='Read-only device DC2DM.EXE path')
    parser.add_argument('--font', required=True, type=Path)
    parser.add_argument('--serial', required=True)
    parser.add_argument('--choice-save', type=Path, help='Native PC save at the MAR_0129_E1 birthday choice')
    parser.add_argument('--transition-save', type=Path, help='Native PC save before the MAR_0129_E1 classroom mask wipe')
    parser.add_argument('--cases', nargs='+', choices=['stages', 'DC2DM01.dat', 'DC2DM02.dat',
                        'DC2DM03.dat', 'DC2DM04.dat', 'DC2DM06.dat', 'rows', 'menus', 'choices', 'transitions', 'minigames',
                        'minigames-suit', 'minigames-reversi-0', 'minigames-reversi-1', 'minigames-reversi-2'],
                        help='Run only selected cases; stages includes all title restart checks')
    args = parser.parse_args()
    if 'choices' in (args.cases or []) and (not args.choice_save or not args.choice_save.is_file()):
        parser.error('--cases choices requires --choice-save with a native MAR_0129_E1 choice save')
    if 'transitions' in (args.cases or []) and (not args.transition_save or not args.transition_save.is_file()):
        parser.error('--cases transitions requires --transition-save with a native MAR_0129_E1 pre-wipe save')
    sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'AppData/Local/Android/Sdk')))
    jdk = Path(os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr'))
    suffix = '.exe' if os.name == 'nt' else ''
    adb = [str(sdk / 'platform-tools' / ('adb' + suffix)), '-s', args.serial]
    run = uuid.uuid4().hex[:12]
    out = repo / 'build/dc2dm-save-validation' / run
    classes = out / 'classes'
    classes.mkdir(parents=True)
    java = jdk / 'bin' / ('java' + suffix)
    sources = [repo / 'tests/android/dc2dm-save-probe/org/dc4engine/Arisa32Activity.java']
    sources += [repo / 'android/app/src/main/java/org/dc4engine/profile' / (name + '.java')
                for name in ('SaveMetadata', 'ScenarioTitles')]
    subprocess.run([str(jdk / 'bin' / ('javac' + suffix)), '--release', '8', '-encoding', 'UTF-8',
                    '-cp', str(sdk / 'platforms/android-36/android.jar'), '-d', str(classes), *map(str, sources)], check=True)
    d8 = sorted((sdk / 'build-tools').glob('*/lib/d8.jar'))[-1]
    subprocess.run([str(java), '-cp', str(d8), 'com.android.tools.r8.D8', '--min-api', '26',
                    '--lib', str(sdk / 'platforms/android-36/android.jar'), '--output', str(out),
                    *map(str, classes.rglob('*.class'))], check=True)
    with zipfile.ZipFile(args.apk) as apk:
        for name in apk.namelist():
            if name.startswith('lib/arm64-v8a/') and name.endswith('.so'):
                (out / Path(name).name).write_bytes(apk.read(name))
    device = '/data/local/tmp/arisa32-dc2dm-save-' + run
    subprocess.run(adb + ['shell', 'mkdir', '-p', device + '/input'], check=True)
    data = device + '/assets/AdvData'
    subprocess.run(adb + ['shell', 'mkdir', '-p', data], check=True)
    # Keep native profile discovery inside the fixture, away from the player's SaveData.
    for name in ('GRP', 'MES', 'MOVIE', 'MUSIC', 'SE', 'VOICE.DAT'):
        subprocess.run(adb + ['shell', shlex.join(['ln', '-s', args.data.rstrip('/') + '/' + name,
                                                 data + '/' + name])], check=True)
    for file in [out / 'classes.dex', *out.glob('*.so')]:
        subprocess.run(adb + ['push', str(file), device + '/' + file.name], capture_output=True, check=True)
    subprocess.run(adb + ['push', str(args.font), device + '/font.ttc'], capture_output=True, check=True)
    for name in ('system.dat', 'DC2DM01.dat', 'DC2DM02.dat', 'DC2DM03.dat', 'DC2DM04.dat', 'DC2DM06.dat'):
        subprocess.run(adb + ['push', str(args.saves / name), device + '/input/' + name], capture_output=True, check=True)
    if args.choice_save:
        subprocess.run(adb + ['push', str(args.choice_save), device + '/input/choice.dat'], capture_output=True, check=True)
    if args.transition_save:
        subprocess.run(adb + ['push', str(args.transition_save), device + '/input/transition.dat'], capture_output=True, check=True)
    command = ['toybox', 'timeout', '-k', '5s', '110s',
               'env', 'CLASSPATH=' + device + '/classes.dex', 'LD_LIBRARY_PATH=' + device,
               'app_process', '/data/local/tmp', 'org.dc4engine.Arisa32Activity', device,
               data, device + '/font.ttc', args.exe]
    digest = hashlib.sha256((out / 'libarisa32.so').read_bytes()).hexdigest()
    log = 'APK SHA256=' + hashlib.sha256(args.apk.read_bytes()).hexdigest() + '\n'
    log += 'libarisa32.so SHA256=' + digest + '\n'
    modes = []
    for case in args.cases or ['stages', 'DC2DM01.dat', 'DC2DM02.dat', 'DC2DM03.dat',
                              'DC2DM04.dat', 'DC2DM06.dat', 'rows']:
        if case == 'stages':
            modes += [mode for stage in range(3) for mode in ('stage' + str(stage), 'stage' + str(stage) + '-restart')]
        elif case == 'minigames':
            modes += ['minigames-suit', 'minigames-reversi-0', 'minigames-reversi-1', 'minigames-reversi-2']
        else:
            modes.append(case)
    status = 0
    (out / 'result.log').write_text(log, encoding='utf-8')
    print('Evidence:', out, flush=True)
    for mode in modes:
        try:
            result = subprocess.run(adb + ['shell', shlex.join(command + [mode])], capture_output=True,
                                    text=True, encoding='utf-8', errors='replace', timeout=120)
            output = process_output(result)
            status = result.returncode
            if status:
                output += '\nFAIL ' + mode + ': remote probe exited ' + str(status) + '\n'
        except subprocess.TimeoutExpired as error:
            output = process_output(error) + '\nFAIL ' + mode + ': adb exceeded 120 seconds\n'
            status = 124
        log += '[' + mode + ']\n' + output
        (out / 'result.log').write_text(log, encoding='utf-8')
        print(mode + ': ' + output, flush=True)
        if status:
            break
    try:
        result = subprocess.run(adb + ['pull', device + '/captures', str(out)], capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=30)
        if result.returncode:
            log += '\nCapture retrieval failed: ' + process_output(result)
            status = status or result.returncode
    except subprocess.TimeoutExpired as error:
        log += '\nCapture retrieval exceeded 30 seconds: ' + process_output(error)
        status = status or 124
    except OSError as error:
        log += '\nCapture retrieval failed: ' + str(error) + '\n'
        status = status or 1
    (out / 'result.log').write_text(log, encoding='utf-8')
    print('Evidence:', out, '\n' + log, flush=True)
    raise SystemExit(status)


if __name__ == '__main__':
    main()

"""Run isolated DC2PC JNI gallery checks; only synthetic artwork is exported as images."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shlex
import struct
import subprocess
import uuid
import zipfile


NAMES = ('iv_otm01a', 'iv_otm01b', 'iv_otm02a', 'iv_otm02b', 'iv_otm02c', 'iv_otm03a', 'iv_otm04a')


def synthetic(root):
    from PIL import Image
    for name in ('MES', 'GRP/BG', 'DAT'):
        (root / name).mkdir(parents=True, exist_ok=True)
    def script():
        return bytearray(b'\0\0\0\0\x69\x99')
    def param(out, op, a, b):
        out.extend(bytes((op, a, b)))
    def words(out, op, *values):
        out.extend(bytes((op,)) + struct.pack('<4H', *(value & 65535 for value in values)))
    def text(out, op, value):
        out.extend(bytes((op,)) + value.encode('ascii') + b'\0')
    basic = script(); param(basic, 9, 0, 0)
    for name in ('main', 'title_00'):
        (root / 'MES' / (name + '.mes')).write_bytes(basic + b'\0')
    gallery = script(); text(gallery, 0x35, 'menu'); text(gallery, 0x30, 'BG/test6')
    for tile, cg in enumerate((0, 2, 5, 6)):
        x = 282 + tile * 115
        words(gallery, 0x4f, x, 107, x + 96, 179); text(gallery, 0x3a, 'cg' + str(cg))
    param(gallery, 9, 0, 0)
    for cg in range(7):
        text(gallery, 0x35, 'cg' + str(cg)); words(gallery, 0x5f, 97, cg, 1, 0); param(gallery, 2, 1, 0)
        text(gallery, 0x30, '*' + str(cg)); words(gallery, 0x5c, 3, 2, cg, 0)
        text(gallery, 0x3a, 'pan' if cg == 5 else 'still'); param(gallery, 0x1e, 1, 1)
    text(gallery, 0x3a, 'cg0')
    text(gallery, 0x35, 'still'); param(gallery, 0xd, 0, 0); text(gallery, 0x3b, 'page_chk')
    param(gallery, 0x12, 0, 1); text(gallery, 0x3a, 'cg_next')
    text(gallery, 0x35, 'pan'); words(gallery, 0x62, 0, -600, 1, 0); param(gallery, 0xd, 0, 0)
    text(gallery, 0x3b, 'page_chk'); param(gallery, 0x12, 0, 1); words(gallery, 0x62, 0, 0, 0, 0)
    param(gallery, 0x12, 0, 1); text(gallery, 0x3a, 'cg_next')
    text(gallery, 0x35, 'page_chk'); text(gallery, 0x3a, 'menu')
    text(gallery, 0x35, 'cg_next')
    for cg in range(7):
        words(gallery, 0x5f, 3, 2, cg, 0); param(gallery, 2, 1, 0)
        text(gallery, 0x3a, 'cg' + str((cg + 1) % 7)); param(gallery, 0x1e, 1, 1)
    (root / 'MES/cg_otome.mes').write_bytes(gallery + b'\0')
    names = bytearray(772); names[0] = 6; names.extend(struct.pack('<3I', 0, 0, 7))
    images = []
    for cg in range(7):
        name = 'BG/test' + str(cg); names.extend(name.encode().ljust(128, b'\0'))
        h = 1200 if cg == 5 else 600
        image = Image.new('RGB', (800, h)); pixels = image.load()
        raw = bytearray(800 * h * 3)
        for y in range(h):
            for x in range(800):
                rgb = ((x // 16 * 7 + cg * 31) % 256, (y // 12 * 5 + cg * 17) % 256, (x + y) % 251)
                pixels[x, y] = rgb
                at = (y * 800 + x) * 3; raw[at:at + 3] = bytes(reversed(rgb))
        crx = bytearray(b'CRXG' + struct.pack('<hh6H', 0, 0, 800, h, 1, 0, 0, 0))
        for at in range(0, len(raw), 8):
            crx.append(255); crx.extend(raw[at:at + 8])
        (root / 'GRP' / (name + '.crx')).write_bytes(crx)
        images.append(image)
    names.extend(struct.pack('<I', 0)); (root / 'DAT/names.dat').write_bytes(names)
    return images


def numeric_samples(images, destination):
    with destination.open('w', encoding='ascii') as out:
        for cg, image in enumerate(images):
            rgb = image.convert('RGB'); pixels = rgb.load()
            for crop in range(0, 601, 2) if cg == 5 else (0,):
                values = [str(cg), str(crop)]
                for y in range(18, 657, 58):
                    for x in range(18, 919, 60):
                        sx = (x + .5) * 800 / 960 - .5; sy = (y + .5) * 600 / 720 - .5 + crop
                        x0, y0 = int(sx), int(sy); fx, fy = sx - x0, sy - y0
                        channels = []
                        for c in range(3):
                            top = pixels[x0, y0][c] * (1 - fx) + pixels[x0 + 1, y0][c] * fx
                            bottom = pixels[x0, y0 + 1][c] * (1 - fx) + pixels[x0 + 1, y0 + 1][c] * fx
                            channels.append(math.floor(top * (1 - fy) + bottom * fy + .5))
                        values.append('%02x%02x%02x' % tuple(channels))
                out.write(' '.join(values) + '\n')


def main():
    repo = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', required=True, type=Path)
    parser.add_argument('--serial')
    parser.add_argument('--data', help='Read-only device AdvData path; linked GRP/MES/DAT only')
    parser.add_argument('--local-data', type=Path, default=Path('E:/Games/DC2PC/AdvData'))
    parser.add_argument('--font', type=Path, default=repo / 'android/app/src/main/assets/fonts/PretendardJP-Bold.ttf')
    parser.add_argument('--expect', choices=('baseline', 'fixed'), default='fixed')
    parser.add_argument('--kinds', nargs='+', choices=('original', 'synthetic'), default=['original', 'synthetic'])
    parser.add_argument('--width', type=int, default=2400)
    parser.add_argument('--height', type=int, default=1080)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--prepare-only', action='store_true', help='Compile and prepare local evidence without adb')
    args = parser.parse_args()
    if not args.prepare_only and (not args.serial or ('original' in args.kinds and not args.data)):
        parser.error('device execution requires --serial, and original routes require --data')
    run = uuid.uuid4().hex[:12]
    out = (args.output or repo / 'build/dc2pc-cgmode-20261108/device' / (args.expect + '-' + run)).resolve()
    if out.exists():
        parser.error('--output must be a new directory to preserve previous evidence')
    out.mkdir(parents=True)
    sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'AppData/Local/Android/Sdk')))
    jdk = Path(os.environ.get('JAVA_HOME', 'C:/Program Files/Android/Android Studio/jbr'))
    suffix = '.exe' if os.name == 'nt' else ''
    android = sdk / 'platforms/android-36/android.jar'
    classes = out / 'classes'; classes.mkdir()
    sources = [repo / 'tests/android/dc2pc-gallery-probe/org/dc4engine/Arisa32Activity.java',
               repo / 'android/app/src/main/java/org/dc4engine/ScreenViewport.java']
    subprocess.run([str(jdk / 'bin' / ('javac' + suffix)), '--release', '8', '-encoding', 'UTF-8',
                    '-cp', str(android), '-d', str(classes), *map(str, sources)], check=True)
    d8 = sorted((sdk / 'build-tools').glob('*/lib/d8.jar'))[-1]
    subprocess.run([str(jdk / 'bin' / ('java' + suffix)), '-cp', str(d8), 'com.android.tools.r8.D8',
                    '--min-api', '26', '--lib', str(android), '--output', str(out),
                    *map(str, classes.rglob('*.class'))], check=True)
    hashes = {'apk': hashlib.sha256(args.apk.read_bytes()).hexdigest(), 'sources': {}}
    with zipfile.ZipFile(args.apk) as apk:
        for name in apk.namelist():
            if name.startswith('lib/arm64-v8a/') and name.endswith('.so'):
                content = apk.read(name); (out / Path(name).name).write_bytes(content)
                hashes[Path(name).name] = hashlib.sha256(content).hexdigest()
    for kind in args.kinds:
        root = out / kind; root.mkdir()
        if kind == 'synthetic':
            images = synthetic(root / 'assets/AdvData')
        else:
            spec = importlib.util.spec_from_file_location('crx_decode', repo / 'tools/proto/crx_decode.py')
            decoder = importlib.util.module_from_spec(spec); spec.loader.exec_module(decoder)
            images = []
            for name in NAMES:
                path = args.local_data / 'GRP/bg' / (name + '.CRX'); content = path.read_bytes()
                hashes['sources'][name] = hashlib.sha256(content).hexdigest()
                image, _ = decoder.decode(content); images.append(image)
        numeric_samples(images, root / 'samples.txt')
        for image in images:
            image.close()
    (out / 'manifest.json').write_text(json.dumps(hashes, indent=2), encoding='utf-8')
    print('Prepared:', out, flush=True)
    if args.prepare_only:
        return
    adb = [str(sdk / 'platform-tools' / ('adb' + suffix)), '-s', args.serial]
    device = '/data/local/tmp/arisa32-dc2pc-gallery-' + run
    subprocess.run(adb + ['shell', shlex.join(['mkdir', '-p', device])], check=True)
    for file in [out / 'classes.dex', *out.glob('*.so')]:
        subprocess.run(adb + ['push', str(file), device + '/' + file.name], capture_output=True, check=True)
    subprocess.run(adb + ['push', str(args.font), device + '/font.ttf'], capture_output=True, check=True)
    log = 'device=' + device + '\n' + json.dumps(hashes, indent=2) + '\n'; status = 0
    for kind in args.kinds:
        target = device + '/' + kind; data = target + '/assets/AdvData'
        subprocess.run(adb + ['shell', shlex.join(['mkdir', '-p', data])], check=True)
        if kind == 'original':
            for name in ('GRP', 'MES', 'DAT'):
                subprocess.run(adb + ['shell', shlex.join(['ln', '-s', args.data.rstrip('/') + '/' + name,
                                                         data + '/' + name])], check=True)
        else:
            subprocess.run(adb + ['push', str(out / kind / 'assets/AdvData') + '/.', data], capture_output=True, check=True)
        subprocess.run(adb + ['push', str(out / kind / 'samples.txt'), target + '/samples.txt'], capture_output=True, check=True)
        command = ['toybox', 'timeout', '-k', '5s', '110s', 'env', 'CLASSPATH=' + device + '/classes.dex',
                   'LD_LIBRARY_PATH=' + device, 'app_process', '/data/local/tmp', 'org.dc4engine.Arisa32Activity',
                   device, data, device + '/font.ttf', kind, args.expect, str(args.width), str(args.height)]
        try:
            result = subprocess.run(adb + ['shell', shlex.join(command)], capture_output=True, text=True,
                                    encoding='utf-8', errors='replace', timeout=120)
            text = result.stdout + result.stderr; status = status or result.returncode
        except subprocess.TimeoutExpired:
            text = 'FAIL adb exceeded 120 seconds\n'; status = status or 124
        log += '[' + kind + ']\n' + text; print(text, flush=True)
        result = subprocess.run(adb + ['pull', target + '/captures', str(out / kind)], capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=30)
        if result.returncode:
            log += result.stdout + result.stderr; status = status or result.returncode
        (out / 'result.log').write_text(log, encoding='utf-8')
    print('Evidence:', out, flush=True)
    raise SystemExit(status)


if __name__ == '__main__':
    main()

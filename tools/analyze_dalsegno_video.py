"""Decode each reference frame and create contact sheets and change metrics."""
import argparse
import csv
import json
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw


def analyze(video, output):
    output.mkdir(parents=True, exist_ok=True)
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open {video}")
    fps = cap.get(cv2.CAP_PROP_FPS)
    frames, samples, changes, previous = 0, [], [], None
    width = height = 0
    while True:
        ok, frame = cap.read()
        if not ok:
            break
        height, width = frame.shape[:2]
        small = cv2.resize(frame, (320, 180))
        difference = np.zeros_like(small) if previous is None else cv2.absdiff(small, previous)
        changes.append((frames, frames / fps, float(difference.mean()),
                        float(difference[:40].mean()), float(difference[130:].mean())))
        previous = small
        if frames % max(1, round(fps)) == 0:
            samples.append((frames, Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))))
        frames += 1
    cap.release()
    with (output / 'frame-changes.csv').open('w', newline='') as target:
        writer = csv.writer(target)
        writer.writerow(['frame', 'seconds', 'mean_delta', 'upper_delta', 'textbox_delta'])
        writer.writerows(changes)
    for page, start in enumerate(range(0, len(samples), 30)):
        sheet = Image.new('RGB', (1280, 1026), '#222222')
        draw = ImageDraw.Draw(sheet)
        for n, (index, frame) in enumerate(samples[start:start + 30]):
            x, y = (n % 5) * 256, (n // 5) * 171
            sheet.paste(frame.resize((256, 148)), (x, y))
            draw.text((x + 4, y + 150), f'{index / fps:06.1f}s / frame {index}', fill='white')
        sheet.save(output / f'overview-{page + 1:02d}.jpg', quality=94)
    metadata = dict(video=str(video), frames=frames, fps=fps, seconds=frames / fps,
                    width=width, height=height, samples=len(samples))
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2))
    print(json.dumps(metadata))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('video', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    analyze(args.video, args.output)

# Da Capo II Dearest Marriage: scene-transition recording

The recording shows a feathered white mask replacing the sunset classroom with the daytime classroom. The outgoing scene is covered from the lower-left toward the upper-right; the incoming scene is then revealed in the same dominant left-to-right direction. The mask has a broad, sloping boundary. The original mask asset should determine that boundary.

This analysis applies only to the **Da Capo II Dearest Marriage** profile. It is evidence from the PC recording, not a claim that the Android implementation or device validation has passed.

Source: [20261107-2338-23.3428059.mp4](C:/Users/adamb/AppData/Local/Packages/Microsoft.ScreenSketch_8wekyb3d8bbwe/TempState/Recordings/20261107-2338-23.3428059.mp4). The file contains **306 frames at 30 fps**, is **10.200 seconds** long, and measures **1018 × 600 pixels** including the Windows title bar. The video codec is H.264, with stereo AAC audio. Its size is 3,648,693 bytes and SHA-256 is `41910da029432ad567eaefca1f425da1ab0075470a0856ca1136861c4d143940`.

All 306 frames were decoded at their recorded dimensions using FFmpeg, saved as JPEG with `q:v 2`, hashed, and measured against the preceding frame. Every frame was visually inspected in consecutive contact sheets; the transition boundaries, dialogue, sprite appearance, and recording artifact were also inspected in individual full-size frames. The JPEGs preserve the source dimensions but are not lossless reference images. Frame numbers below are one-based: `seconds = (frame - 1) / 30`.

| Frames | Time | Observed behavior |
| --- | --- | --- |
| 1–87 | 0.000–2.867 s | Sunset classroom, one character, and pink message box. The line types out as “Rencana ini baru saja disusun beberapa hari yang lalu.” |
| 88–94 | 2.900–3.100 s | The message box disappears at frame 88. The outgoing background and character remain fully visible. |
| 95–150 | 3.133–4.967 s | White progressively covers the scene. Whitening is first measurable at frame 95 near the left/lower-left. At frame 150 a small outgoing remnant remains at the far right. |
| 151–159 | 5.000–5.267 s | The game image is fully white. The title bar and mouse cursor remain outside the game effect. |
| 160–186 | 5.300–6.167 s | The empty daytime classroom emerges from white, beginning at the left/lower-left. At frame 186 a small white contribution remains at the right. |
| 187–201 | 6.200–6.667 s | The daytime classroom is fully revealed and the message box is still hidden. |
| 202–245 | 6.700–8.133 s | The pink message box reappears at frame 202, and narration types over the daytime classroom. |
| 246–248 | 8.167–8.233 s | Four characters appear together: very faint at 246, partially visible at 247, and fully visible at 248. The message box stays visible through this short blend. |
| 249–270 | 8.267–8.967 s | The group remains still while the Akane line types. |
| 271 onward | 9.000 s onward | A brief distortion/pixelation also affects the Windows title bar. This cannot be attributed to an in-game transition. The group/dialogue continues through frame 306. |

The visible white-cover phase lasts approximately **1.867 seconds**, while the visible reveal lasts approximately **0.900 seconds**. The cover is therefore about twice as long as the reveal in this capture. Nine consecutive fully white frames occupy the 0.300-second interval from frame 151 up to frame 160. That interval alone does not establish an intentional script wait: rendering, asset preparation, and capture scheduling can also contribute. Timing precision is limited to the source's 33.3 ms frame interval, and some captured frames repeat.

The outgoing character is covered together with the background. It does not remain visible over the white intermediate state or carry into the empty daytime classroom. The message box disappears before the wipe begins and returns after the reveal finishes. These are separate sequencing requirements from the opacity/mask shape itself.

The short group appearance at frames 246–248 is a second visual event. Its approximately 67 ms span between the first faint and first complete captured frames is an observation, not an exact engine duration. It must not be confused with the longer white classroom change. The distortion starting at frame 271 affects window chrome as well as game pixels, so reproducing it would copy a recording artifact.

The white-alpha measurements use frame 88 as the outgoing reference, frame 151 as white, and frame 201 as the incoming reference. The title bar is cropped before measurement. Pixels with insufficient contrast to white are excluded. Column medians verify the direction and endpoint timings; they do not fully describe the two-dimensional mask or establish an exact blending formula. For example, outgoing frame 121 has estimated white contributions of about 0.93, 0.30, and 0.06 at the left, middle, and right; incoming frame 171 has approximately 0.14, 0.76, and 1.00 respectively. The full original grayscale mask is preferable to a synthetic vertical ramp.

Evidence is saved under [dc2dm-scene-transition-20261108](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108):

| Evidence | Coverage |
| --- | --- |
| [Source manifest](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/source-manifest.json) and [ffprobe metadata](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/probe.json) | Original file identity and video/audio metadata |
| [Frame metrics](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frame-metrics.json) | All 306 frames: timestamps, change measurements, color measurements, and JPEG hashes |
| [White-alpha measurements](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/white-ramp-measurements.json) | Every frame 88–203; outgoing/incoming references and sampled opacity |
| `initial-consecutive-1.jpg` through `initial-consecutive-4.jpg` | Every frame 1–88 |
| `transition-consecutive-1.jpg` through `transition-consecutive-6.jpg` | Every frame 89–212 |
| `dialogue-consecutive-1.jpg` through `dialogue-consecutive-2.jpg` | Every frame 213–239 |
| `group-consecutive-1.jpg` through `group-consecutive-2.jpg` | Every frame 240–286 |
| `ending-consecutive-1.jpg` | Every frame 287–306 |
| [Overview](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/overview-1.jpg) | Quick overview; consecutive sheets above provide complete coverage |

Representative full-size evidence: [outgoing text, frame 75](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00075.jpg), [box hidden, frame 88](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00088.jpg), [first whitening, frame 95](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00095.jpg), [white-cover progression, frame 121](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00121.jpg), [full white, frame 151](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00151.jpg), [first reveal, frame 160](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00160.jpg), [complete reveal, frame 187](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00187.jpg), [message box restored, frame 202](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00202.jpg), [partial group, frame 247](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00247.jpg), [complete group, frame 248](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00248.jpg), and [window-wide recording distortion, frame 271](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/frames/00271.jpg).

Reproduction scripts are [analyze_frames.py](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/analyze_frames.py), [analyze_transition.py](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/analyze_transition.py), and [inspect_remaining_frames.py](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/inspect_remaining_frames.py). The first extracts per-frame statistics from the existing JPEGs, the second produces transition/group sheets and white-alpha measurements, and the third supplies consecutive sheets for the rest of the clip.

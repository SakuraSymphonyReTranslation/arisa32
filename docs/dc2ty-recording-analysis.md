# DC2TY reference recording analysis

The recording covers the opening movie, title animation, Extra screens, configuration, scenario selection, one scenario entry, and a return to title. It supports the native screen sequence and animation timing below. It does not establish save/load behavior, replay execution, progress-dependent title artwork, or renderer texture resolution.

## Source and coverage

- Recording: `C:\Users\adamb\AppData\Local\Packages\Microsoft.ScreenSketch_8wekyb3d8bbwe\TempState\Recordings\20261110-0539-19.2380742.mp4`.
- H.264, 794 x 618 including the window title bar; 30 fps; 8,455 frames; final frame PTS 281.800000 s; video duration 281.833300 s. The title bar occupies approximately 24 pixels. This crop does not establish the exact native canvas size.
- All 8,455 frames were decoded sequentially twice, and both counts matched FFprobe. Per-frame difference, luminance, dark-fraction, regional-change, and full-image CRC32 measurements were collected. Difference measurements sample every second pixel.
- Visual review covered all 283 overview samples in 12 sheets, six detailed transition sheets, and selected full frames/crops. Not every decoded frame was manually inspected. The second pass collected 162 distinct selected frames and retained 62 full PNGs.
- The full audio stream was decoded and measured in 2,818 windows of 100 ms. No speech recognition or listening-based identification of announcements was performed.
- Raw measurements, scripts, intermediate frames, and contact sheets remain in the task-specific temporary folder: `C:\Users\adamb\AppData\Local\Temp\arisa32-dc2ty-video-c70a6ce86bdf4538989a2432cdac29df`. This report retains technical observations only.

Frame indices below are zero-based. Times refer to recording PTS; displayed precision does not imply more than one-frame visual certainty.

## Title sequence

| First observed state | Frame | Time (s) |
| --- | ---: | ---: |
| Sky/rainbow background after final white movie frame | 4313 | 143.766667 |
| Centered title logo | 4346 | 144.866667 |
| Blurred downward school pan | 4374 | 145.800000 |
| School reaches its held position | 4499 | 149.966667 |
| Sharp school background replaces blurred hold | 4529 | 150.966667 |
| Faint corner-logo text | about 4532 | about 151.066667 |
| Main menu | 4551 | 151.700000 |

The sky-to-menu interval is 7.933333 s. The moving pan lasts about 4.167 s, followed by a 1.0 s stationary blurred hold before sharpening. The first faint corner-logo pixels are uncertain by one or two frames because of recording compression; the logo is clearly formed by frame 4534. Roof-template tracking confirms the school is stationary from frame 4499 rather than still moving during the blurred hold.

The main menu contains `To You`, `Continue`, `Extra`, `Config`, and `GameEnd`. Extra replaces the choices in place with `CG Mode`, `Scene Replay`, and `Music Mode`, preserving the school/rainbow background. After returning from a scenario, the same title artwork appears again; no progress flag change or alternative title background is demonstrated.

## Screen sequence and behavior

| Screen/action | Recording evidence |
| --- | --- |
| Extra | First visible at frame 4735 / 157.833333 s; frame 4734 still has the main menu. |
| CG Mode | First visible at frame 4790 / 159.666667 s. Four category buttons, up to a 3 x 3 thumbnail grid, page arrows/counter, and a back button. Full-screen browsing includes tall-image panning and transitions between variants. Return to Extra occurs around 188.4 s. |
| Scene Replay | Entered around 189.5 s. Four categories and two columns of up to five scene buttons; observed category page totals are five, four, three, and three. No replay scene was launched. Return occurs around 201.2 s. |
| Music Mode | Visible by frame 6077 / 202.566667 s. Two columns with 16 entries on the first of three pages. Playback symbols, selected track name, and yellow-green hover feedback are visible. Visual state changes alone do not establish each control's audio semantics. Return occurs around 214.4 s. |
| System Config | First visible at frame 6486 / 216.200000 s. Native blue/turquoise controls include toggles, sliders, and a system-voice portrait. |
| Voice Config | First visible at frame 6559 / 218.633333 s. A 4 x 4 portrait grid, ALL ON/OFF controls, and System return are present. Clicking portraits adds/removes X marks; hovered X marks appear red. Persistence after restart is untested. |
| Scenario Select | Transition starts around 232.4 s. Four story logos on the right change the title, character art, and synopsis on the left. The recording browses Hydrangea, Setsugekka, Hujan yang Turun di Siang Hari, and Hati Sebuah Mesin. One scenario is entered around 239.6 s, with dialogue/background visible around 244.1 s. |
| In-game popup | Visible by 260.4 s, with WIN/FULL, SAVE, LOAD, CONFIG, TITLE, and CLOSE. SAVE and LOAD were not opened. |
| Return confirmation | TITLE hover is green. The `Mau kembali?` Yes/No confirmation appears at frame 7851 / 261.700000 s. Yes hover is yellow; the title transition starts at frame 7921 / 264.033333 s. |
| Returning title | Sky reappears at frame 7978 / 265.933333 s; main menu is visible by 271.8 s. Scenario Select is reopened and other story information is browsed before the recording ends. |

The CG evidence supports preserving the full image composition and tall-image pan behavior. A scaled screen recording cannot establish source texture resolution, GPU texture limits, or which renderer path was used. Native artwork should remain the source for implementation; the capture is a behavioral reference.

## Original opening comparison and audio

The local opening asset is `E:\Games\DC2TY\AdvData\MOVIE\dc2ty_op.mpg`: MPEG-1 video, 640 x 480, average 30000/1001 fps, video duration 143.860378 s, container duration 144.091422 s, with 44.1 kHz stereo MP2 audio. FFmpeg successfully decoded it. This was a decode check, not a GUI player or Android playback test.

The recording's first image best matches original decoded frame 22, approximately 0.734067 s into the movie, among the first 90 original frames searched. Mean absolute RGB error was 5.30485 after resizing the recording content and excluding the cursor. The original's first sampled image is black.

Three-second audio clips sampled at original movie times 10, 30, 60, 100, and 130 s all match the recording with a recording-minus-movie offset of -0.649625 s. Normalized correlations range from 0.94902 to 0.98301. Together with the image comparison, this strongly supports the same original soundtrack and a recording that starts after the movie begins. The image and audio offsets differ; these measurements are not an exact A/V synchronization diagnosis.

Measured RMS is below -55 dBFS from 140.1 to 143.6 s, with activity resuming near the first title background. Other quiet intervals include 149.5-150.2 s and the return-to-title interval 264.4-266.0 s. These measurements do not identify a phrase, voice actor, announcement asset, or particular UI sound.

## Implementation and verification limits

The recording is a reference for title timing, native menu arrangement, screen transitions, configuration feedback, and CG composition. It does not verify Arisa32's implementation. Separate host checks are required for movie natural completion, skipping, decode failure, callback ordering, high-resolution CG loading/panning, save/load screens, replay launch, and persistence of configuration or unlock flags.

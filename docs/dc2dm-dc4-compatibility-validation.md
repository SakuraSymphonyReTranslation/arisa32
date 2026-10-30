# DC2DM and DC4 compatibility verification

Verified on 2026-10-29 and 2026-10-30 with the installed game assets and the connected I2219 Android device.

DC2DM support remains a compatibility beta. The user reports that the game is fully playable, with oversized menu fonts, incorrect notification popup text colors, repeated character voices, and other unresolved UI and compatibility bugs. The checks below do not establish that all routes or UI states are bug-free.

Release 0.6.2 package checks and the fresh desktop/Java results are recorded in [release QA](release-qa-0.6.2.md).

## Changes

- DC2DM Scenario Mode sets replay flag `bank9[7]`. Replay rejects the sidebar, Save/Load, Quick Save/Load, aliases, shortcuts, and pending quick-save confirmations. Disabled hits are consumed without advancing dialogue. QS/QL use the authored gray chips from row 368 of sheet 21, as specified by the native replay branch in `win_size.mes`. Normal story access resumes when replay ends. Internal lifecycle snapshots retain the replay restriction.
- Sakuramusubi DC4PH uses CP932 with a custom word separator. In `dc4_sen20181220d.mes`, dialogue index 33, stored MES byte `5F` becomes `7F` after the format's byte transform. The quote bytes are CP932 `81 75` and `81 76`. The only authored newline is after “lovebirds”; there is none after the opening quote. The renderer recognizes this separator and keeps opening punctuation with following text. Existing CP932 selection is correct; no separate charset is required.
- DC4PH's final D.C. scenario transfers to `MED`, which starts scrolling credits from `GRP/ED/ED.PCK`. Its 22 CRX pages are 1280×721. The previous 720-only validation rejected the archive. Both 720- and 721-pixel pages now retain the native 720-pixel scroll cadence, final hold, fade, and return to `MAIN`.
- DC2DM movie lookup prioritizes `.mpg` across the existing content directories, preserving language precedence among MPG copies. Native `.mp5` files are accepted as a fallback. This preference is scoped to DC2DM movie names; other games retain their overlay precedence.
- Android movie playback now drains the decoder to its actual end of stream. Previously, the duration-plus-two-seconds cutoff could report success while H.264 frames remained undecoded. Due frames are consumed while pumping packets, with only the newest due frame converted for presentation. A decoder that stops producing output during draining reports a stall error instead of silently finishing.
- DC2DM raw slots 2–5 hold four body layers, and slots 6–9 hold their four accessories. Raw `2F` controls the accessory without hiding its body. Accessories inherit the body's position and zoom and render immediately after it. Body removal also releases its accessory; layer reuse cannot resurrect released resources, while an explicit slot reload can use the remembered filename. Raw `6B` mode 0 resets zoom and the zoom anchor when setting position.
- The sidebar cannot open during either credits renderer. Previously it could remain invisible over the ending and pause the animation while BGM continued. Access resumes after completion.

## DC2DM media formats

The active `MOVIE/DC2DM_OP01.mpg`, `DC2DM_OP02.mpg`, and `DC2DM_OP03.mpg` files contain MP4/H.264 High video at 1920×1080, 60 fps, with AAC stereo audio. Android detects the container from its contents. The native backups with `.mp5` extensions contain MPEG-PS/MPEG-1 video and MP2 audio at 1280×720; the local OP01 backup has an `.mkv` suffix but the same MPEG-PS content. No files were renamed or converted.

| Active file | Duration | Full decoded frames | Full decoded audio samples per channel |
| --- | --- | --- | --- |
| DC2DM_OP01.mpg | 126.131 s | 7,565 | 5,562,368 |
| DC2DM_OP02.mpg | 116.169 s | 6,967 | 5,123,072 |
| DC2DM_OP03.mpg | 107.183 s | 6,428 | 4,726,784 |

Independent software decoding established these counts. Device evidence is under `build/dc2dm-check/opening-ending/`: OP01 `backend-d8db0f677e85`, OP02 `backend-34c2a92116c0`, and OP03 `backend-1b2bdbfdab9d`. These runs compiled the changed decoder into an isolated probe linked to the previous packaged library; final package checks are recorded separately. OP02 was tested before the stalled-decoder watchdog was added.

The final decoder also excludes paused time from its drain watchdog. `backend-a520a2ec9172/result.log` records a six-second pause at 5.9 seconds in generated H.264 media: the clock stayed frozen, all 360 frames and 265,216 audio samples were processed, and playback completed without error. The earlier full-opening runs precede this pause-only adjustment. Generated delayed-audio, silent-tail, and control fixtures, plus both original DC4 MPEG movies, passed in `backend-d8db0f677e85/short-cases.json`.

The active OP02/OP03 patch files correspond to the opposite-numbered native backups in the supplied copy. Lookup honors the user's MPG preference and does not swap them. Format metadata and contact sheets are in `local-movie-formats.json` and `opening-format-comparison.png`.

DC2DM's final ending is rendered from `GRP/ED/GED.PCK`, `GEDANIME.PCK`, `ANIME00.PCK` through `ANIME14.PCK`, and `photo.CRX`. Service 208 starts a timing reference before 28 final dialogue commands; service 209 in `MED` displays the credits and montage, followed by the scripted GED02–GED06 and EV_SUC37A images. Its timing depends on the elapsed time since service 208. The asset headers, native pan table, disassembly, and final-script dump are retained in the same evidence directory.

The ending regression checks all 47 montage cards, first-play skip prevention, the restored timing reference, replay skipping to `skip_end`, and the original final-photo pixels. Natural completion reaches `ED` and then `MAIN`; replay returns to `SC_END`. Desktop captures are in `opening-ending/captures/`. The native 16-tick crossfade is represented as approximately 267 ms at 60 Hz; the test does not claim a frame-for-frame comparison against a recorded PC playthrough.

## DC2DM sprite verification

`tests/dc2dm_sprite_test.cpp` checks four body/accessory pairs, position and zoom, pose replacement, hat-only removal, pair removal, explicit reload, background cleanup, interleaved layer order, and snapshot restoration. Pixel checks use distinct colors and an intentional overlap to distinguish the correct ordering from drawing all accessories last.

The original `mar_0201_a1.mes` marker 65 shows Anzu, Nanaka, Akane, and Wataru outside the house. Before the fix, Nanaka's hat floated over the neighboring character; it also survived a removal trigger. The corrected capture positions the hat on Nanaka, and the next dialogue replaces the entire group with Minatsu and Yume. Tests locate the dialogue by its native marker so text patch length does not affect the check. Captures are in `build/dc2dm-check/sprites/before/` and `after/`. Natural overlap authored by the script is retained.

The final packaged Android library passes the same sprite tests with zero failures in `sprites/android-c93e76068e9e/`. Snapshot restoration produces an identical frame on the device before and after advancing to the next dialogue. Compared with desktop, the first frame differs in 133 pixels and the next in 307 pixels, each by at most 1/255 per color component. The comparison permits only that color-rounding tolerance; the position checks and on-device snapshot comparisons remain exact.

An earlier isolated test omitted music track 014 and the current voice clip, so its audio snapshot could not restore. The final fixture includes the original music, the exact WTR004 voice bytes, and the restore UI assets, with source/copy hashes verified. This correction changed the test fixture, not the APK.

## Verification and evidence

All 22 desktop CTest suites passed in 32.47 seconds, including original DC2DM route assets, sprite positions and lifecycle, its 47-card ending, replay access restrictions, mixed Japanese/Indonesian text, the original Sakuramusubi sentence, wrapping, both credit page sizes, and the installed DC4PH D.C. ending flow.

- Final desktop build and complete test run: `build/dc2dm-check/desktop-compat-complete-build.log` and `desktop-compat-complete-test.log`.
- Byte evidence and reproducible English/Indonesian captures: `build/sakuramusubi-check/encoding-evidence.json`.
- The actual Android CP932 decoder maps `7F` to U+001A, unlike the Windows decoder. The phone regression uses the original dialogue bytes and rejects either control character in the visible text. The separator must be normalized before that decoder runs.
- Android replay probe: 9 replay cases, including DC2DM flags 1 and 2, normal story, and existing DC4 cases, plus the Android CP932 space regression. Earlier APKs fail the new DC2DM or decoder assertions.
- Android credits probe uses the packaged production library and original archive. Captures at 0, 5, and 120 seconds, natural completion, and return to `MAIN` passed. The old APK rejects 721-pixel pages.
- Both original `KotoRI_uta02.mpg` and `dc4_mv001.mpg` completed production decoder playback on Android with audio: 420 frames in about 14 seconds and 810 frames in about 27 seconds. The phone's DC4 and DC4PH copies match the tested files. Kotori also completed using the silent-video clock used with separate BGM. Pause, bitmap copy, stride, and a separate nonblack frame-change check passed.

The final APK passed the packaged replay/movie checks in `opening-ending/packaged-1e62f41149bc/manifest.json`. These load its actual arm64 library without a source-backend override. Both the 200 ms polling test and six-second near-end pause decoded all 360 H.264 frames and 265,216 audio samples. The five audio fixtures passed, including full original Kotori and DC4 movies. A separate five-second `dc4_mv001` sample used the silent-video clock and verified a changed, nonblack frame before playback completion. No retries were needed.

The same final library passed the original DC2DM ending test on Android in `movie-review/dc2dm-ending-fdf00dd90c50/`. It covers all 47 montage cards, timer restoration, normal and replay completion, final-photo pixels, and sidebar access before/during/after the ending. Source assets were copied into a private device test directory and verified by SHA256.

The compatibility checks above used `Arisa32-v0.6.1-dc2dm-debug.apk`, containing both arm64-v8a and armeabi-v7a. These are historical package details preceding release 0.6.2. Package/library hashes and comparisons to Gradle output are recorded in `build/dc2dm-check/android-final-artifact.json`; signature verification is recorded in `android-final-signature.log` alongside it.

Final build: `android-compat-complete-build.log`, successful in 5 minutes 13 seconds. Package size is 15,882,342 bytes. APK SHA256 is `7fd7517b6244a3049ddce839a9a30ecf6b4bf9f7abf469e35005e80c0a783364`. Both native libraries match Gradle's stripped outputs, and APK v1/v2 signatures verify.

## Scope of the visual review

Direction comes from the user's original PC screenshots and installed artwork. This is a compatibility repair to those interfaces. It introduces no new visual style, theme, navigation structure, or marketing content. The review covers the changed text, replay controls, sprites, and credits. It does not certify every scene or the original game's accessibility.

Antislop delivery gate, assessed by the primary agent within that scope:

| Item | Status and evidence |
| --- | --- |
| R-02 | PASS: authored report and delivery copy use no em dash. |
| R-03 | PASS: reviewed 1280×720 desktop and Android render targets; the reported sentence and credits fit their viewports. |
| R-17 | PASS: test counts and playback measurements come from execution logs. |
| R-18 | PASS: no testimonials added. |
| R-23 | PASS: supplied game artwork reused; no invented assets or navigation. |
| R-24 | PASS: no website navigation added. |
| R-25 | PASS for changed styles: no new colors or contrast choices; original outlined game text retained. No global WCAG claim. |
| R-26 | PASS: replay controls intentionally reject interaction and consume taps; normal-story persistence has positive controls. |
| R-27 | PASS: existing missing/invalid-archive errors remain; persistence rejects replay before file I/O. |
| R-28 | PASS: no FAQ added. |
| R-32 | PASS for changed access rules: desktop keyboard save/load entry points use the same replay guard; no new keyboard UI. Existing full-menu focus behavior was not redesigned. |
| R-33 | PASS: fixes are in runtime source, without a runtime source-patching script. |
| R-34 | PASS: no theme toggle or theme change. |
| R-35 | PASS for changed interactions: desktop and both Android ABIs built; replay entry points, normal controls, text rendering, credits progression, and movies executed. |
| R-36 | PASS: no security, compliance, or broad performance claims. |
| R-37 | PASS: original PC/game assets are the user-supplied direction. |
| R-38 | PASS: text and assets derive from the installed games. |
| R-01 | PASS: original game gradients retained for faithful rendering; none added. |
| R-04 | PASS: original action icons retained. |
| R-06 | PASS: existing game font and metrics used to preserve patch layout. |
| R-07 | PASS: no decorative background introduced. |
| R-08 | PASS: no decorative arrows introduced. |
| R-09 | PASS: no badges introduced. |
| R-10 | PASS: no new glass effects. |
| R-12 | PASS: no new shadows. |
| R-13 | PASS: no new glow. |
| R-14 | PASS: no feature-card layout introduced. |
| R-19 | PASS: credits restore the authored scrolling behavior and timing. |
| R-22 | PASS: only original game illustrations. |
| Liveliness | PASS: ENERGY 2 / RHYTHM 2 / MOTION 2 describes the supplied scene artwork, distinct message/menu regions, and restored scripted motion; no visual redesign. The scene or credits remain the focal point. |
| C-1 | PASS: each change serves a reported compatibility defect. |
| C-2 | PASS for scope: replay restrictions, normal access, text, and credits have execution evidence. |
| C-3 | PASS: no filler sections or product screens. |
| C-4 | PASS for scope: positive and negative replay cases, both credit page heights, native decoder paths, and normal text fixtures covered. |
| C-5 | PASS: findings are tied to bytes, source, captures, and execution logs. |
| R-05 | PASS: original game composition preserved. |
| R-11 | PASS: original control geometry preserved. |
| R-15 | PASS: existing concrete game actions retained. |
| R-16 | PASS: no marketing terminology added. |
| R-20 | PASS: original DC2DM/DC4PH visual identity preserved. |
| R-21 | PASS: no theme decision introduced. |
| R-29 | PASS: original game palette preserved. |
| R-30 | PASS: no unrelated product interface copied. |
| R-31 | PASS: rendering decisions reproduce the supplied games and patch text. |

## Limits

The Android probes exercise the actual packaged native library and Android decoding/rendering services. They are not a full installed Activity lifecycle or complete route playthrough. DC2DM's three opening flows were exercised through 40–45 dialogue lines, with portable save-state restoration. Native PC DC2DM save import is not supported; DC2DM uses Arisa32 portable saves. Complete backlog portrait mapping remains unverified. No user game files or existing saves were modified.

The desktop movie backend remains MPEG-1 only. The MP4 contents of the active DC2DM MPG patches require the Android FFmpeg backend. Decoded-frame counts establish complete processing, not presentation at 60 fps; a slow render loop coalesces overdue frames.

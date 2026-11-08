# DC2DM choices and per-line voice replay

Scope: Da Capo II Dearest Marriage only. The shared DC2DM choice renderer covers every story choice using the profile, including choices outside the supplied Anzu example. DC2PC, DC4, and other profiles retain their existing positioning and replay policy.

## PC reference

Reference recording: `20261107-1228-55.2948850.mp4`, 718 frames at 30 fps, 1024 × 574. Extracted frames and analysis are in `build/dc2dm-choice-av-20261107/`.

The supplied screenshot and recording show two 612 × 60 native choice rows at y = 180 and 240. `start.mes` supplies five row templates, the normal pink art, purple hover art, pressed art, floral borders, caption coordinates, and click rectangles. The renderer repositions the templates as one centered group and keeps hit testing aligned with the displayed rows. All three interaction states use these native assets. The background dims behind the choices while the message window remains visible.

All 718 decoded frames have per-frame hashes and image-difference measurements in `frame-metrics.json`. The ten `review-*.jpg` contact sheets were visually reviewed, followed by enlarged inspection of the entry, press, and exit frames in `choice-critical-frames.jpg`. This is a complete frame analysis with detailed inspection of the changing states, not a claim that every frame was separately viewed at full size.

| Event | First occurrence | Second occurrence |
| --- | --- | --- |
| Choices first visible | Frame 258, 8.5667 s | Frame 593, 19.7333 s |
| Selected row pressed | Frames 370 through 372 | Frames 614 through 615 |
| Choices removed | Frame 373, 12.4000 s | Frame 616, 20.5000 s |

Times use `(frame - 1) / 30`. The preceding frames have no choice rows. The recording shows an immediate entry/exit, pink normal rows, purple hover, and a pale pressed fill with the floral border retained. It does not show a separate multi-frame choice fade. The measured background brightness ratio is 0.4937, consistent with the implemented 50% dimming and video quantization. Dimming occurs before drawing the message window.

The final caption correction retains the native logical size 22, fixed grid, and tracking -1. Translated Latin glyphs use raster size `22 * 15 / 19`, matching the installed translation DLL's separate glyph-size adjustment. Native font evidence is in `docs/dc2dm-native-backlog.md` and `docs/dc2dm-native-config-preview.md`. Width fitting uses those same advances, so the caption and its measured width agree.

At normalized native resolution, the first caption spans 305 pixels in both the PC recording and final renderer; the previous renderer spanned only 241. PC bright-glyph bounds are `[317,198,621,211]`, compared with `[314,198,618,212]` in the final renderer. The remaining three-pixel horizontal bearing difference and one-pixel height difference reflect the portable rasterization; this is not a pixel-identical GDI reproduction. Both final rows were visually checked for complete text, circle markers, border alignment, and clipping. Evidence: `choice-caption-final-comparison.png`, `choice-caption-final-metrics.json`, and `captures/choice-0-final.png` / `choice-1-final.png`.

## Branch evidence

The recorded choice is in `MAR_0129_E1`:

| Choice | Label offset | First Yoshiyuki dialogue | First subsequent voice |
| --- | ---: | --- | --- |
| Memberi selamat dengan jujur | `label01`, 20322 | Anzu. Sekali lagi, selamat ulang tahun ya | `MAR_0129_E1_ANZ022` |
| Mencoba sedikit bercanda | `label02`, 22380 | Anzu. Gimana rasanya tambah tua setahun lagi? | `MAR_0129_E1_ANZ028` |

Both paths jump to `cmn` at 24164. The shared dialogue begins at 24379: “Semuanya, sekali lagi terima kasih buat hari ini”, using `MAR_0129_E1_ANZ031`.

The VM already reads each raw `43` target followed by its raw `50` caption. No branch order is inferred from the caption or rewritten by this change. Regression tests select each row from a restored choice state and check its destination. An additional real-data test checks both recorded branches through their common continuation.

The static audit of all 311 disassemblies found 18 choice groups in 17 scripts, with 47 choices total: seven groups of two and eleven groups of three. All 47 targets resolve to labels in their own scripts. The complete mapping is recorded in `build/dc2dm-choice-av-20261107/choice-audit.json`. This establishes target coverage; it is not a claim to have played every path to completion.

## Voice ownership

Static inspection of 279 story scripts found 39,740 text commands. All 8,435 lines whose speaker is exactly Yoshiyuki have no associated raw `35` or `36` voice playback command. Some joint-speaker lines containing Yoshiyuki also contain another character's voice, so speaker-name filtering would be incorrect.

Raw `37` preloads a future voice; it does not make that voice belong to the current line. Existing audio handling clears the replay association at raw `06`, retains a played voice after playback completes, and ignores preload commands for replay ownership. The DC2DM toolbar now follows that association: an unvoiced line has an inactive replay button, with no hover or pressed response. Restoring an unvoiced line must keep it inactive.

## Completed validation, 2026-11-07

The earlier host tool timeouts and build-environment problem were resolved sufficiently to build and test this artifact. They did not establish an APK/game crash. Logs are retained under `build/dc2dm-choice-av-20261107/`.

| Check | Result and evidence |
| --- | --- |
| Desktop build | PASS with MSVC 14.44.35207 and Windows SDK 10.0.26100.0. `validate.ps1` records the required environment. |
| C++ regressions | All 27 test targets have passing results across the initial suite and focused reruns. The initial run passed 26/27; `scene_test` failed two test-text assumptions about a fullwidth question mark and an embedded newline. Corrected assertions then passed in `scene-validation.log`. These were test expectations, not branch-routing changes. |
| Final caption regression run | PASS 4/4: `scene_test`, `dc2pc_menu_test`, `dc2pc_flow_test`, and `text_render_test`, after the last production edit. See `caption-validation.log`. |
| Java profile regressions | PASS: 328 profile checks in this session, including game-profile isolation. |
| Android build | PASS: `assembleDebug` completed in 4m 3s. `android-build-final.log` and `.exit` (0). |
| APK verification | PASS: debug package `org.dc4engine`, version 0.6.9 / code 16, minimum API 21, target API 36; valid v1/v2 signatures; arm64-v8a and armeabi-v7a libraries. `apk-signature-final.log` and `apk-badging-final.txt`. |
| Final packaged JNI on device | PASS: 14 cases on serial `10DF1N05QN000H8`, using the final APK's arm64 library. `android-probe-final.log` and `.exit` (0). |
| Save preservation | PASS: all eight original PC SaveData files and all fourteen phone SaveData files retain their pre-validation SHA-256 hashes. Before/after manifests are in the same evidence directory. |
| Patch whitespace | PASS: `git diff --check`. |

The build used the existing offline init script `build/release-0.6.9/baseline-agp.init.gradle`, which resolves the cached AGP 8.5.2. The user's unrelated `android/build.gradle` change to AGP 8.13.2 was preserved. Gradle reported SDK XML and deprecation warnings, but completed successfully.

`tests/scene_test.cpp` verifies one through five rows, every fixture branch, hit rectangles, normal/hover/pressed rendering, invalid indices, save restoration, both real Anzu branches, and their common continuation. `tests/audio_test.cpp` verifies replay after playback completes, unvoiced narration/Yoshiyuki, and restored unvoiced state. A voiced joint-speaker line remains replayable. The production renderer, row count/offset, caption metrics, dimming, and unvoiced-toolbar policy are all gated by `dc2dm_ui_`; DC2PC and other profiles retain their existing behavior.

The final Android probe's evidence is `build/dc2dm-save-validation/0d28ac5455d3/`, containing `result.log` and 53 captured files. It exercises actual JNI DOWN/MOVE/UP/CANCEL events, both choices, normal/hover/pressed art, cancellation, release over another row, outside taps, correct Anzu voice names, inactive Yoshiyuki/narration replay, and restored unvoiced saves. The other cases recheck menu pages and four Otome voice selections, full CG at 1920x1080 and next/back navigation, title particles and pause/resume, three title stages and restart persistence, five native save/companion round trips, and save-row actions. The root reviewer also inspected the final device pressed-state capture.

The probe loads packaged JNI in a separate `app_process` fixture. It does not install or replace the user's application. Generated saves remain in its isolated `/data/local/tmp/arisa32-dc2dm-save-0d28ac5455d3/` directory; original game data is read-only. This validates native behavior on the phone, not a complete Activity, audio-listening, or manual gameplay pass. The 32-bit library was built but was not run on a 32-bit device. The full story was not played through every branch. Earlier menu/CG/title and story-progression evidence remains in `docs/dc2dm-menu-audio-transition-validation.md` and `docs/dc2dm-save-progress-validation.md`.

## Debug artifact

- Delivery file: `build/apk/Arisa32-0.6.9-debug-DC2DM-choices-20261107.apk`.
- Build output: `android/app/build/outputs/apk/debug/app-debug.apk`.
- Size: 16,261,645 bytes; build timestamp 2026-11-07 22:16:54, Asia/Bangkok.
- APK SHA-256: `9e52c57cbc32eed3e3a8c90ca9705ee5a3ab58193b6a0e72563f8737fc3b89dc`.
- Tested arm64 `libarisa32.so` SHA-256: `bb0dc0f5d5af231a8b605c4e8897dd8e15fd7db2781d6da55b4a33b2bb555895`.

For manual device testing, select the Da Capo II Dearest Marriage profile, load the Anzu birthday prompt, try both choices from the same save, and check the immediate Yoshiyuki line followed by Anzu. Replay should be inactive for an unvoiced line and active for Anzu's own recording. Check normal, hover/cursor, pressed, and cancelled touches as applicable to the selected input mode.

## Delivery Gate: PASS

Root-owned review of the changed DC2DM choice renderer and replay controls. Direction is the supplied PC recording and native `start.mes` / CRX artwork. ENERGY 2 / RHYTHM 1 / MOTION 1 describes the fixed floral rows and direct interaction states; previously validated CG/title animations are preserved. This gate covers the changes and tested states described above, not an accessibility certification or a complete manual playthrough of the original game.

| Check | Result and concrete evidence |
| --- | --- |
| R-02, punctuation | PASS: no decorative em dash or new product prose introduced. |
| R-03, containment | PASS: 1-5-row rectangle checks and final 1280x720 desktop/device captures keep captions and borders within the game canvas. |
| R-17, statistics | PASS: 718 frames, 47 targets, 305px caption width, and test counts come from the stored analyses and logs. |
| R-18, testimonials | PASS: none introduced. |
| R-23, assets | PASS: the requested restoration uses original game sheets, font, and audio; no invented visual assets. |
| R-24, navigation | PASS: all 47 script labels resolve; restored real choices reach their respective branches and common continuation. |
| R-25, contrast | PASS for the changed caption style: white glyphs retain the black outline, a 21:1 color pair; final captures were inspected for legibility. Original artwork is retained as requested. |
| R-26, controls | PASS: both real choice rows and voiced/unvoiced replay states pass desktop and packaged-JNI interaction checks. |
| R-27, states | PASS: normal/hover/pressed, cancel, outside release, wrong-row release, invalid indices, and restored silent lines are covered. |
| R-28, FAQ | PASS: none introduced. |
| R-32, keyboard | PASS within scope: existing desktop numeric choice selection is retained; input routes still use the tested selection method. No new host focusable element was added. Physical Android keyboard navigation was not retested. |
| R-33, implementation | PASS: production behavior is implemented in repository C++; probe scripts load the built library and do not patch it. |
| R-34, themes | PASS: no theme behavior introduced or changed. |
| R-35, execution | PASS: final desktop/Android builds and affected tests pass; actual JNI touch sequences ran on the final packaged library. Activity/manual-play limits are stated above. |
| R-36, claims | PASS: rasterization differences, frame-review method, test coverage, and runtime limits are explicit. |
| R-37, direction | PASS: the user supplied the original PC appearance before implementation; the final states were compared to those frames. |
| R-38, content | PASS: captions, branches, and voice identifiers come from the installed game's MES data. |
| R-01, gradients | PASS: native row fills retained to match the requested game UI. |
| R-04, icons | PASS: original circle/floral/replay artwork retained; no unrelated icon set. |
| R-06, typography | PASS: fixed 22px grid and translated glyph scale follow native/DLL evidence; final caption width matches the PC measurement. |
| R-07, patterns | PASS: native floral borders identify the original DC2DM selection UI. |
| R-08, arrows | PASS: no decorative arrows introduced. |
| R-09, badges | PASS: none introduced. |
| R-10, glass | PASS: no new glass effect; the existing translucent row artwork is required by the reference. |
| R-12, shadows | PASS: no new component shadows; text outline preserves legibility. |
| R-13, glow | PASS: original selected-row border retained to identify the active choice. |
| R-14, repeated elements | PASS: repeated rows represent equivalent branch choices and share the native 60px pitch. |
| R-19, motion | PASS: no unobserved entry/exit animation added; pressed and hover states follow the measured frames. |
| R-22, illustrations | PASS: only original game artwork used. |
| Dials | PASS: 2/1/1 declared for the choice UI's existing visual emphasis, fixed rhythm, and direct states. |
| Dial consistency | PASS: the group stays fixed while only the active row changes state. |
| Focal point | PASS: 50% scene dimming places focus on the available choices. |
| Whitespace | PASS: the row group is centered using `240 - count*30`, with shared art/caption/hit-test offsets. |
| Accent | PASS: purple hover and pale press distinguish the active native pink row. |
| Identity | PASS: DC2DM floral sheets, circle markers, and fixed-grid captions remain visible in the final captures. |
| Design read | PASS: native game UI restoration follows the supplied reference, with no new visual direction substituted. |
| C-1, intent | PASS: every changed position/state/metric restores observed PC behavior or prevents stale voice replay. |
| C-2, function | PASS: both branches and replay states run through the final APK's JNI library. |
| C-3, composition | PASS: the renderer draws the script's actual choices; no filler interface added. |
| C-4, resilience | PASS in tested scope: 1-5 choices, cancelled/mismatched/outside touches, saved choice restoration, and unvoiced restoration pass. |
| C-5, evidence | PASS: decoded frames, native-script mapping, captures, logs, save manifests, and APK/library hashes are recorded. |
| R-05, layout | PASS: original story-choice composition retained. |
| R-11, shapes | PASS: native row shapes retained because they are the requested reference. |
| R-15, actions | PASS: original story captions and replay actions retained. |
| R-16, copy | PASS: no marketing copy introduced. |
| R-20, identity | PASS: final native art and caption grid identify DC2DM. |
| R-21, dark mode | PASS: no theme added; temporary scene dimming matches the recorded choice state. |
| R-29, palette | PASS: original pink/purple/pale-press palette retained. |
| R-30, originality | PASS: no unrelated application template introduced. |
| R-31, rationale | PASS: position, dimming, typography, and interaction states each cite native data or the PC recording above. |

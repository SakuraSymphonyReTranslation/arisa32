# DC2DM menu audio, CG preview, and title transition

Date: 2026-11-07. Local development build after v0.6.9; not a published release.

## Scope and behavior

The changes select the detected DC2DM profile in the native menu renderer and Android/desktop hosts. DC4 and DC2PC keep their previous voice maps and gallery effects. Reusing the shared transition object for a normal fade resets the DC2DM particle mode. Existing save/progress changes in the working tree are retained.

- Menu announcements use their own DM_SYS asset and the selected Otome A/B/C voice bank. Nothing disables announcements. Config redraws no longer play every button's attached voice.
- Successful Save and Load use their confirmation voices. Opening Save is silent in the supplied recording. Android waits for the accepted Load voice before adopting restored scene audio, and waits for Quit's voice before leaving.
- Full CG preview renders from the original CRX at the requested 16:9 size, with interpolation instead of the old nearest-pixel 1280x720 intermediate. This preserves the available source detail.
- Next/previous CG follows the current category's native script order, includes unlocked variants, skips locked entries, and wraps within that category. Returning to thumbnails restores the page containing the last viewed entry.
- DC2DM fades from the thumbnail menu into the first CG, then from the previous CG into the next. The default duration is 320 ms; native effect-speed and effects-off settings are honored. A tap during the fade completes it without skipping another image.
- Returning from a scene to Title uses the game's EFF.CRX circle atlas over the captured outgoing frame, then reveals Title. Pause preserves elapsed transition time.

## Recording and asset analysis

The first supplied MP4 contains 3,800 video frames at 30 fps, 1024x600, duration 126.677 seconds. All frames were decoded for frame/time analysis. The game occupies 1024x576 below the 24-pixel window bar. Detailed consecutive-frame strips around frames 1950..2039 and 2540..2659 establish the return-to-title effect. The observed circle sequences are approximately 65.267..66.300 and 85.567..86.500 seconds.

The second MP4 contains 211 video frames at 30 fps, 1024x578, duration 7.033 seconds. Every frame was decoded and all eight contact sheets inspected. The recording shows Married life thumbnails, switches to Dear to around 1.8 seconds, and crossfades into full CG around frames 116..125 (3.867..4.167 seconds). A variant changes around 4.4 seconds. The next transition starts at frames 204..210 (6.8..7.0 seconds); the clip ends during that transition. A completed last transition is not present in the recording.

All 579 listed CG assets have native dimensions 1024x576: 146 Graduation days, 229 Married life, and 204 Dear to. A 1920x1080 preview therefore scales the original detail; it does not turn the source into new HD artwork. No replacement or generated CG assets are introduced.

### Menu voice evidence

Waveform matches above 0.92 correlation identify the following events in the first recording. Lower-correlation search candidates were not used as matches. Letters indicate the selected style, not different menus.

| Event | Matched asset | Time (seconds) |
| --- | --- | --- |
| Music mode | DM_SYS11C | 1.330 |
| Scenario mode | DM_SYS09C | 4.940 |
| CG mode | DM_SYS10C | 9.240 |
| Config | DM_SYS13C | 20.250 |
| Otome style B sample | DM_SYS16B | 27.470 |
| Otome style C sample | DM_SYS16C | 30.400 |
| Otome style A sample | DM_SYS16A | 33.230 |
| Start | DM_SYS03A | 45.470 |
| Graduation days | DM_SYS04A | 48.240 |
| Married life | DM_SYS05A | 72.610 |
| Dear to | DM_SYS06A | 92.020 |
| Open Load | DM_SYS07A | 113.940 |
| Load accepted | DM_SYS15A | 116.140 |
| Save accepted | DM_SYS14A | 124.370 |

The native scripts additionally assign Extra=08, Mini Game=12, Quit=20, and voice-volume sample=21. Host event numbers are translated explicitly to these asset numbers. In particular, 06 belongs to Dear to and must not be used for opening Save. Bank 100[56] selects A=0, B=1, C=2, Nothing=3.

EFF.CRX is an 800x32 atlas of 25 cells. Its decoded RGB is black for both filled and transparent pixels, so circle masking must use alpha. A real-asset regression now checks shrinking occupied area and the eventual black frame. The movement uses deterministic per-tile velocities with the native 40 ms clock. The original executable's random particle positions are not reproduced pixel for pixel.

## Story order and ending

The chronological flow is **Graduation days → Married life → Dear to…… → credits**. Completing every CG or Scenario thumbnail is not a condition for playing the credits.

| Native script evidence | Meaning |
| --- | --- |
| SCESELECT / sce01 → MAR_0101_F1 | Graduation days entry is available from the start. |
| SCESELECT, offset 1353, checks 100[1]; sce02 → OP02 → SIN_0326_F1 | Before Graduation is complete, Start goes directly to Graduation after confirmation. Completing Graduation opens the two-story selector, including Married life. |
| MAR_0326_B1, offsets 39209/39230/39260 | Finishing Graduation sets 100[1], plays OP02, and continues into Married life. |
| SCESELECT / sce03 requires 100[2] | Dear to unlocks after completing Married life. |
| SIN_1931_F2, offsets 39341/39362/39398 | Sets 100[2], plays OP03, and continues into SUC60_0811_A1_OTM. |
| SUC75_1208_A1_OTM, offset 39153 → MED | The last Dear to scene transfers to the ending. |
| MED, offset 43, raw 1B/209 | Starts the authored credit animation, followed by the final photograph and ED. |
| ED → MAIN | Returns to Title after the ending sequence. |

The catalogue has 279 scenario entries: 58 MAR, 128 SIN, 93 SUC. These include alternative branches. Analysis of 311 MES files found 13 cross-file choice forks in these story families; their alternatives rejoin the main flow. The entry count is not a list of 279 mandatory chapters.

Correction from the November 8 Start audit: the presence of the `sce02` label does not make it reachable before the `100[1]` gate. Once the selector is unlocked, Title Start must open its two or three choices without an additional New Game confirmation.

Replay mode (9[7] != 0) can return through SC_END rather than continuing the normal story. ED also checks the sum of flags 100[1..3] before setting 100[0], but that check happens after MED's credits. No write to 100[3] was found in the inspected scripts; executable-side behavior or a legacy field remains unresolved. No invented unlock requirement was added for this field.

## Verification

- Desktop CTest: 27/27 passed. After correcting the real atlas alpha mask, all three affected tests passed again: dc2dm_menu_test, dc2pc_menu_transition_test, and save_host_test. The final desktop build also passed after adding the accepted Auto Load voice.
- Java profile suite: 328 checks passed for this run.
- Native menu tests compare the complete 1024x576 CG frame with the decoded CRX pixels, check 1920x1080 interpolation, reject incorrect aspect ratios, and exercise native category order, locked skips, wrap, effects-off, rapid taps, and returning to the correct thumbnail page.
- Real DC4 All Age and DC4 Plus Harmony gallery tests passed: full-image scaling, original pixels, mask transition, unlock route, rapid taps, cancel/Back, and settings. The first simultaneous run collided on the existing test's fixed temporary profile filename; Plus Harmony passed when rerun alone. The general CTest run does not enable every optional installed-game route suite.
- Android arm64: all 13 cases passed using the packaged JNI library on device 10DF1N05QN000H8. These cover menus/voices/CG/title particles, three title stages with separate-process restarts, five native/companion save round trips, and translated Save/Load row actions.
- Inspected Android captures show the four distinct Otome selections, complete CG at 1920x1080 and its next variant, circle breakup, black boundary, and restored Title. The native tests establish exact timing; captured device frame numbers include PNG output time and are not frame-rate measurements.
- Normal git diff --check passed. No owned .dc2dm-edit-tmp files remain.

### Tested artifact and evidence

- APK: android/app/build/outputs/apk/debug/app-debug.apk, 16261448 bytes, debug version 0.6.9.
- APK SHA-256: e92675ac13fce1aabcbbfc383b540512dbc333ebae7495cb714119ed338e249f.
- Packaged arm64 libarisa32.so SHA-256: 7fbabc66242f02558ad52efedd80feed6f3dbf4a83974fbb7c2d0718e4fe4725.
- [Android result log](../build/dc2dm-save-validation/5525b17c7ea3/result.log).
- [Desktop CTest](../build/dc2dm-menu-av-20261107/ctest-final.log), [atlas regression](../build/dc2dm-menu-av-20261107/mask-checks.log), and [Java checks](../build/dc2dm-menu-av-20261107/java-checks.log).
- [DC4 gallery](../build/dc2dm-menu-av-20261107/dc4-gallery-regression.log) and [Plus Harmony gallery](../build/dc2dm-menu-av-20261107/dc4ph-gallery-serial.log).
- [Android build](../build/dc2dm-menu-av-20261107/android-mask-build.log). The repository's existing AGP edit is retained; this build used build/release-0.6.9/baseline-agp.init.gradle to select the available offline baseline.
- [Menu/style captures](../build/dc2dm-save-validation/5525b17c7ea3/menu-review.jpg), [Android transition](../build/dc2dm-save-validation/5525b17c7ea3/transition-review.jpg), and [PC/desktop transition comparison](../build/dc2dm-menu-av-20261107/particle-comparison.jpg).
- [Voice matches](../build/dc2dm-menu-av-20261107/voice-matches.json), [story analysis](../build/dc2dm-menu-av-20261107/story-analysis.json), and [story graph](../build/dc2dm-menu-av-20261107/story-graph.json).

### Original save comparison

The Android probe writes only to its unique /data/local/tmp fixture and loads copies of PC saves. All 15 original phone files still match the earlier backup manifest. Six of ten PC files match that earlier manifest. The four differing PC files are DC2DM02.dat (last modified 10:24:12 UTC), DC2DM03.dat and DC2DM04.dat (10:24:38 UTC), and system.dat (11:45:43 UTC), all on 2026-11-07. The first recording includes a native Save action around 10:24 UTC. A baseline captured immediately before that PC session is not available, so the earlier manifest is not evidence that all PC files stayed unchanged throughout this work. These newer PC files were preserved without restoration or overwrite. See [original-preservation.json](../build/dc2dm-menu-av-20261107/original-preservation.json).

Local evidence is under build/dc2dm-menu-av-20261107 and build/dc2dm-cg-av-20261107. Reproduction instructions are in tests/android/README.md. These build directories are ignored by Git.

## Limits

- The Android probe exercises the APK's packaged JNI through app_process. It does not install the APK or establish a complete Activity lifecycle/physical-speaker test.
- Audio mapping is supported by native-script inspection, recorded waveform matches, and action tests. A new audible Android recording has not been made.
- Original particle artwork, timing, and shrinking behavior are used; individual particle trajectories approximate the original RNG.
- CG source resolution is 1024x576. Interpolation removes avoidable nearest-neighbor blockiness without inventing image detail.
- This analysis establishes script flow and the tested ending path, not a manual playthrough of every story branch or patch version.

## Delivery gate for the changed UI

Direction: the supplied PC recordings, native DC2DM menu sheets, CGs, and effect atlas. ENERGY 2 / RHYTHM 1 / MOTION 2: retain the fixed game layout and restore short authored transitions. Scope is the changed voice actions, CG presentation/navigation, and return-to-title effect. Existing game artwork and unrelated application controls are not being redesigned or certified for accessibility.

| Check | Result and evidence |
| --- | --- |
| R-02, punctuation | PASS: no host marketing copy or decorative dash was added; report facts identify native labels and measured events. |
| R-03, containment | PASS: exact 1024x576 and 1920x1080 CG render checks pass; invalid aspect ratios are rejected. Native menu layout remains 16:9. |
| R-17, statistics | PASS: frame/asset/test counts come from decoded inputs and stored test logs; no UI statistics added. |
| R-18, testimonials | PASS: none introduced. |
| R-23, assets | PASS: existing CRX and voice assets are used for the specifically requested restoration; no invented artwork. |
| R-24, navigation | PASS: category route wrap, locked-entry skipping, next/previous, and thumbnail return pass native tests. |
| R-25, text contrast | PASS within scope: no text renderer, text color, or label artwork changed. The original game's typography is outside this change. |
| R-26, controls | PASS: menu/style actions, CG taps, next/back, and title return run in the native tests and Android probe. |
| R-27, states | PASS: tests cover locked CGs, disabled effects, rapid taps during transition, missing atlas fallback, pause, and finished state. |
| R-28, FAQ | PASS: none introduced. |
| R-32, keyboard | PASS within scope: no focusable host element introduced; existing desktop gallery input routes to the tested scroll/back actions. Physical Android keyboard use was not rechecked. |
| R-33, implementation | PASS: runtime behavior is written in repository C++/Java; validation scripts do not patch the running APK. |
| R-34, themes | PASS: no theme toggle or theme style added. |
| R-35, execution | PASS: desktop and Android build; packaged JNI runs the menu, CG, and transition case. Full Activity validation is explicitly outside the probe. |
| R-36, claims | PASS: source resolution, RNG approximation, and speaker/Activity limits are stated. |
| R-37, direction | PASS: original game/video direction was retained before implementation. |
| R-38, content | PASS: displayed assets and voice lines come from the game; story conclusions cite MES scripts. |
| R-01, gradients | PASS: none added; crossfade interpolates two real frames to match the recording. |
| R-04, icons | PASS: no generic icon set added. |
| R-06, type | PASS: native sheets and existing font path retained. |
| R-07, pattern | PASS: circles use the game's EFF atlas for the requested title transition. |
| R-08, arrows | PASS: no decorative arrows introduced into the interface. |
| R-09, badges | PASS: none introduced. |
| R-10, glass | PASS: none introduced. |
| R-12, shadows | PASS: none introduced. |
| R-13, glow | PASS: none introduced. |
| R-14, repeated elements | PASS: thumbnail arrangement remains the native category/page layout; full CG occupies the original aspect ratio. |
| R-19, motion | PASS: CG fade communicates image replacement; circle breakup communicates return to Title. Both follow supplied reference behavior and pause correctly. |
| R-22, illustration | PASS: only original game images are displayed. |
| Dials | PASS: 2/1/2 declared above for restoring short game-authored motion without new composition. |
| Dial consistency | PASS: menu composition stays fixed; animation occurs only at the requested boundaries. |
| Focal point | PASS: full CG is the preview focus; native Title/menu focus is retained after transition. |
| Whitespace | PASS: full-frame aspect and original menu spacing retained. |
| Accent | PASS: native selection accent retained, with no additional host accent. |
| Identity | PASS: native pink menu sheets, Otome recordings, and circle atlas retain the game's identity. |
| Design read | PASS: implementation uses the user-supplied PC recordings and game data as its reference. |
| C-1, intent | PASS: interpolation preserves image detail; voice mapping identifies each action; transition restores the referenced behavior. |
| C-2, function | PASS: affected action, preview, and transition routes pass the menu tests and device probe. |
| C-3, content | PASS: no template sections or invented content added. |
| C-4, resilience | PASS in tested scope: pause/resume, effects-off, locked entries, route wrapping, and normal-transition reset pass. |
| C-5, evidence | PASS: logs, source references, captures, and artifact hashes accompany the measured results. |
| R-05, composition | PASS: existing game menu structure retained. |
| R-11, shape | PASS: no new host component shape introduced. |
| R-15, actions | PASS: original CG, Scenario, Music, Save, Load, Config, and Quit actions retained. |
| R-16, copy | PASS: no marketing language added. |
| R-20, identity | PASS: native DC2DM art/audio retained. |
| R-21, dark mode | PASS: no theme change. |
| R-29, palette | PASS: original palette retained. |
| R-30, originality | PASS: no unrelated product template introduced. |
| R-31, rationale | PASS: every visual change restores the requested PC behavior or removes avoidable scaling artifacts. |

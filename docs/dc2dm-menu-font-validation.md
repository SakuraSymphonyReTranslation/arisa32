# DC2DM menu transitions and bundled font validation

Validated on 2026-11-08. This update removes the extra host transition when DC2DM opens its story selector or minigame script, includes story labels in the full-screen fade, and clears selector controls when an opening begins or is interrupted. The compatibility paths require the DC2DM profile or ClassicDM MES version `0x9D72`.

The optional bundled font is available in the existing per-game font picker. The supplied file's real name is **Pretendard JP Bold**. Selecting it changes only the selected game's setting; existing selections and the MS Gothic default remain intact.

## Recording evidence

- [Title and selector analysis](dc2dm-title-selector-frame-analysis.md): all 910 Android frames and 646 PC frames were reviewed in sequence.
- [Minigame entry analysis](dc2dm-minigame-entry-frame-analysis.md): all 549 frames were reviewed in sequence. The Android host added a 1,200 ms wait before a script that already defines its transitions.
- [Font identity and license](bundled-pretendard-font.md): the bundled bytes match the HP's `Kirikiroid/Da Capo 5 Sweet Happiness/default.ttf`, SHA256 `2386cac04bdbe169c8243e55eac43d59c5d634a3776acf3865c406c99d7c259e`.

The recordings have variable capture timestamps and no input log. They establish the visible delay and stale artwork, not exact tap latency. The PC recording shows a three-story selector and ordinary gameplay return; it does not establish a reference animation for a two-story selector, selector Back, or opening interruption.

## Implemented behavior

- Title and Extra provide their current framebuffer to the DC2DM script. The script starts its authored transition immediately, and that menu image is available when the transition mode requires it. The host's separate Start delay is removed.
- Story labels are composed before the native transition, so background, logo, copyright, and labels fade together. A completed black frame survives an authored internal DC2DM script transfer, preventing the Graduation handoff from briefly showing Title again.
- Selector entry retains the active Title BGM and adopts its metadata. The script's same-track mode 1 command therefore continues the track instead of restarting or overlapping it. Mode 0 and different-track commands retain their existing behavior.
- Opening interruption captures the actual movie frame and fades it to Title. Dynamic selector controls and pending movie state are cleared. A late Java completion callback cannot resume the cancelled DC2DM opening. Ordinary gameplay retains its existing circular return effect.
- Minigame entry follows the native `new` or `rnew` path selected by the script. The script still controls Otome's introduction, suit/Reversi selection, and their timing. Forced Title restores Load and discards the interrupted session's results.
- SDL receives the same source composition. Seeded DC2DM entry copies fade-speed variable `99:12` before the first advance and starts the animation clock after loading, so loading time cannot consume the fade.
- The font picker lists `Pretendard JP Bold`, deduplicates an identical imported `default.ttf`, and preserves its saved path when the dialog is merely opened or cancelled. Explicitly selecting the named row uses the bundled copy in `fonts/bundled`. Font bytes and SIL OFL 1.1 notices are included in the APK.
- DC2DM startup loads existing read-history keys before persisting imported PC progress. This prevents initialization from writing an empty `read-lines.txt`. The guard is `SystemMenu::is_dc2dm()`; other profiles retain their previous initialization order.

## APK and builds

Artifact: [Arisa32-v0.7.0-dc2dm-menu-font-debug.apk](../build/dc2dm-title-selector-20261108/Arisa32-v0.7.0-dc2dm-menu-font-debug.apk).

| Property | Verified value |
| --- | --- |
| Package | `org.dc4engine` |
| Version | `0.7.0`, code `17`, debuggable |
| File size | 17,413,749 bytes |
| APK SHA256 | `823d6a2b273438f525a943575290415b6636d0cf9c6f61df109486f06e7f125a` |
| arm64 library SHA256 | `e5917c5c95d75155ae857653690766f0d82053ecc8c8980d285275209ef5eecb` |
| armeabi-v7a library SHA256 | `020f76b5c12655d4d7e24a19836ec22d3a011b9922a6ac360736b2386916f5e0` |

Both Android ABIs and the Java app built successfully. APK checks passed for ZIP integrity, package/version, debug flag, v1/v2 signatures, alignment, staged native library equality, font/license bytes, and absence of probe classes. Evidence: [combined build log](../build/dc2dm-title-selector-20261108/build-combined-android.log), [final build log including read-history fix](../build/dc2dm-title-selector-20261108/build-read-history-android.log), [artifact manifest](../build/dc2dm-title-selector-20261108/apk-verification.json), [signature log](../build/dc2dm-title-selector-20261108/apk-signature.log), and [alignment log](../build/dc2dm-title-selector-20261108/apk-alignment.log).

All **32 desktop tests passed**, in 176.63 seconds: [test log](../build/dc2dm-title-selector-20261108/desktop-combined.log). Coverage includes exact transition composition, toolbar exclusion, internal black-frame persistence, direct-load/reset/cross-profile guards, large elapsed updates, and BGM sample continuity without channel overlap. The final SDL-only timing corrections were compiled separately: [SDL build log](../build/dc2dm-title-selector-20261108/sdl-seeded-entry-build.log). They do not change the APK or tested core library.

## Device runtime

Device: iQOO Z9x 5G / I2219. The isolated native probe used the exact final APK's arm64 libraries and the new bundled font. It did not replace or clear the main application.

All **13 device phases passed**, exit 0:

| Phase | Observed result |
| --- | --- |
| `read-history-pc-profile` | Two existing read keys survive initialization and shutdown while importing PC progress into an existing Android profile. |
| `read-history-pc-new` | The same keys survive PC import when the Android profile is created for the first time. |
| `read-history-profile-only` | Keys survive restart from an existing profile without a PC `system.dat` file. |
| `minigame-entry` | Immediate native script ownership; authored incoming fade; Otome introduction; suit and Reversi choices; forced Title restores Load and preserves the pre-session counters. |
| `stage0`, `stage0-restart` | Initial Start confirmation and cancellation remain correct before Graduation is complete. |
| `stage1`, `stage1-restart` | Two-story selector opens without a redundant prompt or black menu gap, including after restart. |
| `stage2`, `stage2-restart` | Three-story selector opens without a redundant prompt or black menu gap, including after restart. |
| `selector-story0` | Full composition fades to exact black, then the authored Graduation white/scene/dialogue sequence appears without stale Title content. |
| `selector-story1` | Married Life opening supplies an actual textured movie frame; forced Title fades that frame uniformly, clears labels, and ignores a simulated late completion. |
| `selector-story2` | Dear to opening passes the same movie-frame, fade, Title, and late-completion checks. |

Evidence: [result log](../build/dc2dm-save-validation/8d6c0ca1a700/result.log), [captures and frame CSVs](../build/dc2dm-save-validation/8d6c0ca1a700/captures), [frame metrics](../build/dc2dm-title-selector-20261108/frame-metrics.json), [selector contact sheet](../build/dc2dm-title-selector-20261108/device-visual-check.png), and [final minigame/selector/opening contact sheet](../build/dc2dm-title-selector-20261108/history-fixed-final-probe-sheet.png).

The 17 CSVs contain **420 sampled runtime frames**. The measured shared-background ratio during selector entry stays at or above `0.9981068`, with no inserted black frame. Root inspected the final contact sheet: two/three story rows stay within the canvas, labels dim with the rest of the composition, Graduation does not flash Title, and interrupted openings retain the tree/cloud movie image during the fade without selector fragments.

The minigame fixture enters the native `new` path, which reveals the incoming Otome scene from black. Requiring an outgoing fade for every entry was an incorrect initial test assumption. The final assertion follows this authored path and excludes the dialogue overlay from the completed fade measurement. CSV summaries also include endpoint, dialogue, and returned-Title rows, so their unfiltered maximum error is not the maximum within the fade assertions. This fixture correction did not require a production change.

Earlier device checks on the prior selector candidate also passed menus, transitions, and choices: [earlier result log](../build/dc2dm-save-validation/b19506e4ac0c/result.log). Those are supplemental regression evidence, not additional phases claimed for the final APK.

## Font picker checks

The isolated `org.dc4engine.updateqa` application passed **32 font checks**. Its production font, notices, and arm64 native library matched the menu/font candidate `ed89d209...`. The subsequent read-history fix changed only the native libraries and signature entries: every DEX file, font, and license entry is byte-identical between that candidate and the installed final APK. See [final APK comparison](../build/dc2dm-title-selector-20261108/final-apk-change-identity.json). The separate QA package prevents its fixture library entries and imported fonts from changing the user's main application.

Checks cover duplicate naming, preservation of the imported path on Open/Cancel, explicit bundled selection, Android loading the Bold face, effective engine font path, library reload, another profile's unchanged setting, System default, MS Gothic, other imported fonts, and an unset profile's default. English, Indonesian, and Japanese dialogs were exercised at a 2.0 text scale, with a visible target of at least 44 dp, an untruncated font name, and keyboard selection using Tab, D-pad Down, and Enter. Back and Cancel were also exercised.

Evidence: [instrumentation log](../build/bundled-font-20261108/font-qa-instrumentation.log), [QA artifact identity](../build/bundled-font-20261108/qa-artifacts.json), and [screenshots](../build/bundled-font-20261108/captures). Root inspected the final three screenshots: the new name fits, the focus state is visible, and no toast obscures it. Initial test setup was corrected to leave Android touch mode before keyboard selection, keep the QA window awake, and drain queued test toasts before screenshots. No production change was required by these fixture corrections.

## Coverage limits

This is targeted compatibility validation, not a complete playthrough of every scenario or minigame outcome. SDL was compiled but not exercised interactively. The Java movie fallback's late callback was simulated; actual fallback video playback was not exercised. The phone tests cover arm64; armeabi-v7a was built and packaged but not run on a separate 32-bit phone. Frame timestamps and sampled captures do not establish exact PC/Android timing parity.

## Save audit and installation

All game fixtures use separate test data. The final probe's paired recursive hashes show no changes to the 10 PC or 16 HP player save files: `pc-saves-before-history-fixed-final-probe.json`, `pc-saves-after-history-fixed-final-probe.json`, `phone-saves-before-history-fixed-final-probe.txt`, and `phone-saves-after-history-fixed-final-probe.txt` are in [the validation directory](../build/dc2dm-title-selector-20261108).

The oldest HP baseline at 09:54 and its 10:04:16 comparison are identical. By the next pre-test snapshot at 10:19:39, `read-lines.txt` was empty and `system-profile.dat` had a different hash. All 14 other HP files and all 10 PC files remained unchanged. Every subsequent paired selector test, final test, and installation snapshot preserves the state captured at 10:19:39. The logs do not identify what launched or wrote the main game during that earlier interval, so this report does not claim the entire session matches the oldest baseline.

The audit independently reproduced a startup defect in the prior APK using two synthetic read keys: importing PC progress emptied both keys before initialization completed. [Reproduction log](../build/dc2dm-save-validation/2f538de2b833/result.log). The final APK fixes that initialization order for DC2DM, and all three read-history regression cases pass. This prevents future loss through that path; already-empty live history was not restored from an older, nonmatching backup. No live save file was overwritten by a fixture or recovery operation.

The final APK was installed with `adb install -r` successfully. The installed `base.apk` SHA256 exactly matches `823d6a2b273438f525a943575290415b6636d0cf9c6f61df109486f06e7f125a`; package version is `0.7.0` / `17`. The 10 PC and 16 HP files are byte-identical immediately before and after this installation. The main app's `library.properties`, containing per-game settings, also retains SHA256 `e54490164ca52f3570a4ab77e580f8e9eb8be6d26beb22b2ab3bbe41b83f05d2`. Evidence: [installation verification](../build/dc2dm-title-selector-20261108/installation-verification.json) and [install log](../build/dc2dm-title-selector-20261108/install.log). The main game was not launched or cleared during installation. No release was published or replaced.

## Scoped Delivery Gate

Root performed this gate for the changed DC2DM transitions, the added font option, and this report. The direction is the supplied PC game artwork and existing Arisa32 font picker. Game presentation uses ENERGY 2 / RHYTHM 1 / MOTION 2: illustrated scenes, fixed menu regions, and short script-defined transitions. The font dialog uses ENERGY 1 / RHYTHM 1 / MOTION 1: the existing list, selection accent, and immediate interaction. This review does not certify the unchanged legacy artwork for accessibility or claim coverage beyond the states listed above.

### Hard Gate

| Item | Status and evidence |
| --- | --- |
| R-02, prose | PASS: new report and picker label contain no em dash; the font name comes from metadata. |
| R-03, layout | PASS in scope: runtime contact sheets contain the two/three story rows; 2.0-scale font checks confirm the new row is visible and untruncated in all three languages. |
| R-17, numbers | PASS: recording counts, sampled frame counts, test totals, and hashes have linked evidence. |
| R-18, testimonials | PASS: no testimonials or fictional people introduced. |
| R-23, assets | PASS: game artwork is retained; the user explicitly requested the supplied font, whose bytes and license are recorded. |
| R-24, navigation | PASS: stage/restart, all three story choices, selector return, and forced Title paths passed the device probe. |
| R-25, contrast | PASS for the changed scope: the focused font row samples `#FFFFFF` on `#303740`, 12.03:1; original PC bitmap artwork is preserved by user direction. No blanket WCAG claim is made. |
| R-26, controls | PASS: the new named font row persists and resolves the selected path; default, system, imported, Cancel, Back, and story/minigame controls were exercised. |
| R-27, states | PASS: unset/imported/bundled font states, native menu loading, fade endpoints, restart, forced return, and late completion are covered. |
| R-28, FAQ | PASS: no FAQ introduced. |
| R-32, keyboard/focus | PASS in scope: font QA used Tab, D-pad Down, Enter, and Back with a visible selected row; existing game input paths are retained without a new pointer-only control. |
| R-33, source | PASS: behavior is implemented directly in Java/C++; evidence scripts do not patch production source at runtime. |
| R-34, themes | PASS: no theme toggle or replacement theme introduced; the existing game/picker appearances remain. |
| R-35, execution | PASS: both APK ABIs built; 32 desktop tests, 13 final device phases, and 32 font checks passed; captures, final SDL compile, and installed APK hash are linked above. |
| R-36, claims | PASS: the removed 1,200 ms host wait is identified in code; video timing and platform coverage limitations are explicit. |
| R-37, direction | PASS: the user supplied PC recordings and asked to add this exact font to the existing picker. |
| R-38, content | PASS: labels, artwork, font identity, and script behavior have actual game/font sources. |

### Purpose-Gate

| Item | Status and evidence |
| --- | --- |
| R-01, gradients | PASS: no decorative gradient added; whole-frame blending implements the authored scene change. |
| R-04, icons | PASS: no new generic icons; native game controls and existing radio choices remain. |
| R-06, typography | PASS: Pretendard JP Bold is the exact requested file, named from its metadata; it remains optional per game. |
| R-07, patterns | PASS: no background grid or decorative pattern introduced. |
| R-08, arrows | PASS: no decorative arrows added. |
| R-09, badges | PASS: no badges introduced. |
| R-10, glass | PASS: no added glass treatment; existing dialogue artwork remains. |
| R-12, shadows | PASS: no new component shadow. |
| R-13, glow | PASS: no new glow or pulsing decoration. |
| R-14, cards | PASS: the font extends the existing option list; the game retains its authored story rows. |
| R-19, motion | PASS: script-defined fades replace the redundant host wait; full-frame and interruption checks verify their purpose. |
| R-22, illustration | PASS: no generated illustration; scene and movie images come from game assets. |

### Liveliness

| Item | Status and evidence |
| --- | --- |
| Explicit dials | PASS: game 2/1/2 and picker 1/1/1 are stated above. |
| Consistency | PASS: inspected captures retain fixed game regions, script transitions, and the existing picker list. |
| Focal point | PASS: story selection, incoming Otome scene, and the focused font row are the active decisions in their respective screens. |
| Structural space | PASS: native story-row spacing and the existing font-dialog list/padding are retained. |
| Accent | PASS: native game hover artwork and the picker's selected radio/focus highlight identify the active choice. |
| Identity | PASS: DC2DM sakura/title artwork and Arisa32's existing navy/cyan picker remain recognizable. |
| Design reference | PASS: the supplied PC UI and existing picker established the implementation direction. |

### Craftsmanship and Quality Locks

| Item | Status and evidence |
| --- | --- |
| C-1, intention | PASS: the host wait duplicated script work; full composition prevents stranded labels; the actual font name removes filename ambiguity. |
| C-2, completeness | PASS: the named row is selectable, persisted, reloadable, and usable by the engine; the affected menu paths passed runtime checks. |
| C-3, composition | PASS: no extra screen or decorative section was added to the product. |
| C-4, resilience | PASS within tested scope: restart, interruption, large elapsed updates, cross-profile guards, retained read history, per-game font isolation, and large text/keyboard cases are covered. |
| C-5, evidence | PASS: source review, builds, runtime tests, frame evidence, and limitations are separated above. |
| R-05, structure | PASS: native menu regions and the existing Android list are retained without a template layout. |
| R-11, shapes | PASS: no new component shape or radius system. |
| R-15, labels | PASS: story names retain their meaning and the added row identifies the precise font face. |
| R-16, language | PASS: no marketing copy introduced. |
| R-20, identity | PASS: original DC2DM assets remain game-specific and the launcher retains its existing appearance. |
| R-21, theme | PASS: no arbitrary theme change; the user asked for behavior parity and one font option. |
| R-29, palette | PASS: the new row uses the established picker palette; original game artwork is unchanged. |
| R-30, reference | PASS: the PC appearance is the user's explicit reference. |
| R-31, rationale | PASS: original artwork/layout preserve the game; native transitions remove duplicated timing; the supplied optional typeface preserves user choice; a separate bundled directory protects it from imported-name collisions. |

# DC4 display settings validation

Current update: MS Gothic and the system font now share the 18–22 size chooser
with custom fonts. The fixed-size restriction described in this earlier report
is superseded by [FXAA and MS Gothic validation](fxaa-legacy-validation.md).

Validated on 2026-11-04. Scope: D.C.4 and D.C.4 Plus Harmony ambient sakura,
per-game Sakura On/Off, and custom dialogue font sizes.

## Delivered behavior

- Open the game's three-dot settings in the library. DC4/DC4PH show
  **Dialogue font size** and **Sakura petals**.
- Sakura defaults to On, including libraries saved before this setting existed.
  Off hides the ambient falling petals in the title menu and scripted scenes.
  Finite flower transitions retain their existing behavior.
- A selected custom font can use size 18, 19, 20, 21 or 22. MS Gothic and the
  system font remain at 22. The fixed-size explanation links to Choose font.
- Settings are saved per game and take effect when the game is reopened.
  The dialog headings and confirmation messages say this explicitly.
- DC2DM does not receive these DC4-specific settings.

The title and scene paths share `SakuraSystem`, use the game's original atlas,
and respond to the script's slot 28 / effect setup. The simulation advances at
60 Hz. This is not a claim that Android displays 60 frames per second.

## PC recording review

Source: `20261104-0741-30.1466561.mp4` in the user's ScreenSketch recordings.
The window title identifies D.C.4 Plus Harmony. All 1,104 frames were extracted
at 30 fps (36.8 seconds), with 1,103 consecutive-frame comparisons and contact
sheets. Local evidence is in `build/dc4-sakura-font/video-audit/`.

- `summary.json` and `frame-motion.csv` record coverage and image differences.
- `title-tracked-petal.png` shows a small petal across consecutive frames
  189 through 207 (6.30 to 6.90 seconds). It falls while drifting left and
  changing shape. The thresholded component has a median size of 7 by 5
  recorded pixels, moving approximately 44.65 pixels left and 74.60 down.
  These are image measurements, not source-engine coordinates.
- `scene-intervals.png` covers the tree scene and subsequent background changes.
  Cursor movement, background fades/pans and large flower transitions are
  separate from the tiny ambient petals.
- Automated scene tracking produced false positives: the longest candidate
  in `scene-tracked-petal.png` follows changing background detail around frame
  885 onward. It was rejected on visual inspection. Its values in
  `petal-tracks.json` are candidates, not validated petal measurements.

This review establishes the small falling-petal behavior on the recorded title
screen. It does not establish exact scene-petal trajectories or pixel-identical
PC/Android output. Scene animation and switch behavior were verified separately
with the native renderer tests below. Random particle state is not synchronized
to the PC recording.

## Tests and results

| Check | Result and evidence |
|---|---|
| Desktop scene regression | PASS, `build/dc4-sakura-font/scene.log`. Sizes 18 through 22 change rendered output for DC4/DC4PH, Latin and non-Latin text; bounds, reload and save restoration preserve the active preference. |
| Desktop effects | PASS, `build/dc4-sakura-font/effects.log`, failures=0. Ambient and finite effects, On/Off and save restoration, with original assets from both games. |
| Title animation | PASS for both game data sets, `dc4-menu.log` and `dc4ph-menu.log`. On frames animate; Off frames exclude ambient petals. `dc4-menu-comparison.png` and `dc4ph-menu-comparison.png` were visually inspected. |
| Existing voice/progression regression | PASS using `build/dc4-voice-layer/check.cmd`, covering audio, scenes, backlog, DC4 and DC4PH progression. |
| Host preferences | PASS, 17 standalone `DisplayPreferencesChecks` assertions for defaults, persistence, isolation, missing/malformed values and limits. |
| Android dialogs | PASS, 298 assertions in the final run, `build/dc4-sakura-font/device-ui-final.log`. An earlier run passed 256; the final run additionally covers Choose font and Cancel actions. |
| Android native effects | PASS, failures=0 against the release APK library; `build/dc4-sakura-font/device-effects.log` and `build/sakura-validation/android-995094c994df/`. |
| Android builds | PASS for QA app/instrumentation and release arm64-v8a/armeabi-v7a; `qa-build.log`, `qa-final-check-build.log` and `release-build.log`. |
| Installation | PASS, `adb install -r`; installed production APK SHA-256 matches the delivered artifact. Activity launch returned `Status: ok`. |

The Android dialog test uses isolated package `org.dc4engine.updateqa`, the
actual Activity and a private test library. Touch selects On/Off and every size;
button listeners exercise Choose font and Cancel. Keyboard Enter activates a
focused size row after Tab leaves touch mode. Back dismisses without changing
the stored choice. This is a focused selector test, not a full-app accessibility
audit. Thirteen screenshots cover default/custom font dialogs and EN/ID/JP
at 200% system text in `build/dc4-sakura-font/device-ui-captures/`. Visible rows
have no horizontal clipping or ellipsis and tap heights are at least 44 dp.
Long lists scroll. A transient toast from a preceding test can appear over a
later screenshot because these tests switch dialogs rapidly.

The native device probe measured software drawing at 38.262 ms average across
its finite-wipe sequence, 0.505 ms baseline, 50.742 ms during wipes and 9.462 ms
during tails. It excludes screen presentation and is not a display FPS benchmark.

Known unrelated check: the older `LibraryChecks` archive harness stops at
`wrong game rejected`, before reaching its preference assertions. Its strict
archive-identity expectation predates the current importer; see the existing
note in `tests/android/README.md`. The independent 17 preference checks pass.
The full archive harness is not reported as passing.

## Installed artifact

- APK: `build/dc4-sakura-font/Arisa32-v0.6.6-dc4-display.apk`
- Production package: `org.dc4engine`, version 0.6.6, code 13.
- Device serial: `10DF1N05QN000H8`.
- APK SHA-256: `5af12e7614f69287f4131b722b19cac48a81bd4ba5b2d6bb30be5d28ccb0f7a9`.
- Tested arm64 `libarisa32.so` SHA-256:
  `1e09a376f03127559962e13a5bb1d993b12f12648e957c30fd1c83f49f2a9c75`.

Only test code/documentation changed after the release APK was built and
installed. Reproduction commands for the dialog and preference checks are in
`tests/android/README.md`. Local desktop commands are retained in
`build/dc4-sakura-font/check.cmd`.

## Antislop delivery gate

Scope is the two added settings and their dialogs. Direction is the existing
Arisa32 dialog design, with original game artwork and authored game effects.
ENERGY 1 / RHYTHM 1 / MOTION 1 apply to the settings. The user explicitly asked
for looping in-game petals; that authored effect is outside the settings' motion
dial. The following evidence does not claim an audit of unrelated app screens.

### Hard gate

| Item | Result and evidence |
|---|---|
| R-02 | PASS: new labels contain no em dash. |
| R-03 | PASS: device row bounds/ellipsis checks and large-text screenshots show scrolling without horizontal escape. |
| R-17 | PASS: new UI contains size values and saved states, no promotional statistics. |
| R-18 | PASS: no testimonials or invented people were added. |
| R-23 | PASS: existing game assets and existing settings navigation are reused; no new visual assets were generated. |
| R-24 | PASS: both settings rows open their respective dialogs; Choose font opens the existing picker. |
| R-25 | PASS: contrast checker on the dialog background #131B26 gives #E6EDF5 14.68:1, #8899AC 5.94:1, #38BDF8 8.08:1 and white 17.32:1. |
| R-26 | PASS: On/Off, all five sizes, Choose font, Cancel and Back behavior are recorded in the device harness. |
| R-27 | PASS: default font explains the fixed size and offers Choose font; selections show saved/relaunch feedback; malformed stored sizes have tested defaults and bounds. No asynchronous loading step was added. |
| R-28 | PASS: no FAQ was added. |
| R-32 | PASS: native focused list selection responds to keyboard Enter; Back dismisses. Scope is the tested selector, not all app keyboard routes. |
| R-33 | PASS: feature behavior is in Java/JNI/C++ sources, with no runtime source-patching step. |
| R-34 | PASS: these dialogs use the existing fixed Arisa32 palette; no new theme toggle or mode was introduced. |
| R-35 | PASS: builds, 298 device assertions and production launch are recorded above. |
| R-36 | PASS: timing is reported as measured software drawing, without FPS or compliance claims. |
| R-37 | PASS: the existing Arisa32 design and original game assets supplied direction before editing. |
| R-38 | PASS: visible values correspond to actual persisted settings; synthetic fixtures are confined to QA. |

### Purpose gate

| Item | Result and evidence |
|---|---|
| R-01 | PASS: new dialogs inherit a solid background; no gradient or glow added. |
| R-04 | PASS: text labels use no generic decorative icons. |
| R-06 | PASS: native dialog typography preserves the app; game font controls expose the user's requested reading sizes. |
| R-07 | PASS: no grid or patterned background added. |
| R-08 | PASS: no decorative arrows added. |
| R-09 | PASS: no badges added. |
| R-10 | PASS: existing opaque dialog surface, no added glass effect. |
| R-12 | PASS: existing modal elevation only, no new shadow system. |
| R-13 | PASS: no glow added. |
| R-14 | PASS: radio rows share spacing so numeric choices are easy to compare; no feature cards added. |
| R-19 | PASS: settings have no added animation; looping game petals implement the explicit request and can be disabled. |
| R-22 | PASS: original game atlas reused, no generic illustration. |

### Liveliness

| Item | Result and evidence |
|---|---|
| Dials | PASS: ENERGY 1 / RHYTHM 1 / MOTION 1 stated for settings. |
| Consistency | PASS: calm, uniform native selector rows; screenshots show the existing dialog style. |
| Focal point | PASS: each dialog names the setting above its choices. |
| Whitespace | PASS: existing row padding separates selectable values and preserves touch targets. |
| Accent | PASS: existing cyan highlights dialog actions and the app caption. |
| Identity | PASS: Arisa32 caption/palette and original CIRCUS game assets remain recognizable. |
| Design Read | PASS: existing Arisa32 settings language was selected before editing. |

### Craftsmanship and quality locks

| Item | Result and evidence |
|---|---|
| C-1 | PASS: inherit dialog colors/layout/spacing for continuity; radio choices expose the bounded size and binary effect decisions. |
| C-2 | PASS: added controls have tested selection, dismissal or picker behavior. |
| C-3 | PASS: only the two requested per-game settings were added. |
| C-4 | PASS: tested default/custom, On/Off, reload, three languages, large text and focused keyboard activation; other device configurations were not exhaustively tested. |
| C-5 | PASS: assertions, hashes and timing come from retained test output; rejected video candidates are disclosed. |
| R-05 | PASS: established settings list continues; no unrelated page template. |
| R-11 | PASS: existing dialog and native radio shapes preserved. |
| R-15 | PASS: Choose font and Cancel name their actual actions. |
| R-16 | PASS: new copy contains no marketing claims. |
| R-20 | PASS: game-specific sakura and MS Gothic behavior fit the emulator's existing interface. |
| R-21 | PASS: existing palette retained, no new dark-mode decision. |
| R-29 | PASS: existing neutral surfaces/text and cyan action accent reused. |
| R-30 | PASS: follows this app's established design, no external product clone. |
| R-31 | PASS: colors preserve identity, list layout compares choices, native typography supports system scaling, and existing spacing preserves touch targets; no cards/icons/illustrations added. |

UI supplement: PASS within this scope. Palette and accent follow Arisa32;
no decorative emoji, badges, template shapes, invented data or added motion
appear in settings. Choices expose actual state and work through their native
listeners. The fixed-font explanation supplies the relevant next action.
Large-text and keyboard coverage is bounded by the device checks described above.

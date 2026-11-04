# D.C.4 / D.C.4 Plus Harmony visual effects

The per-game **Visual effect** On/Off setting controls falling sakura and
floating lights together. It is available in Manage game and in the in-game
settings sheet. Changes during play run on the engine thread and take effect
without reopening the game. Existing `sakuraEnabled=false` preferences migrate
to Off; an explicit `visualEffectsEnabled` value takes precedence.

## Authored triggers and save restoration

The renderer now handles ambient `0x28` types 1, 9 and **11** as sakura, and
type **13** with the original `SYSTEM/EFFYOUSEI` atlas. Type 17 names a different
asset, `EFFHOTARU`; it is not the circular effect identified in these samples.
Effects start and stop only when the script commands request them. No outdoor,
background-name or time-of-day rule was added.

The PH screenshot dialogue about returning from the Wonderland parade belongs
to `dc4_mid20190402e.mes`: BG038C loads at position 2204, `0x28(13,2)` follows
at 2239, and the quoted dialogue is at 2434. The script corpus also starts
`0x28(13,2)` after BG038B in `dc4_asu20190506c.mes`, position 4496, and
`dc4_asu20190505b.mes`, position 24818. BG038B was decoded and visually checked:
it is the sunset version of this street. Thus the afternoon observation is
supported by game data, despite the broken afternoon recording.

Native executable evidence retained in `build/sakura-trigger/`:

- PH dispatch around `0x42AAA5` selects `system\effyousei`; the particle
  initializer is `0x41CCD0`, and its update branch is `0x41D704..0x41D8D8`.
- The common five-layer initializer is `0x41F300`. Types 1, 9, 11 and 13 share
  these buffers. A light start or stop therefore replaces/clears petals.
  The earlier claim that type 13 leaves petals running was incorrect.
- Each layer has `{0,1,3,6,9,12,20}` particles according to the clamped units
  digit of the parameter. Light parameter 2 produces 15 particles in total.
  Light opacity changes by 0.005 per 60 Hz tick between 0 and 0.8; vertical
  speed is signed 0.2..0.8 pixels/tick; horizontal position includes a sine
  offset. The atlas advances after 500 ms, using a 32px stride and 28px crop.
  The final light row is clipped to the actual 164px sheet height.
- Sakura also uses the native 32px horizontal stride. Its former 28px stride
  sampled across cell boundaries. The simulation preserves authored density
  and wind; finite screen flowers and mask wipes remain separate.
- Both executables copy native slot data from global state +`0x4B80`, with
  length `0x56FE4`. Effect fields +`0x40C84`, +`0x40C88`, +`0x40C90` therefore
  map to file offsets `0x3C104`, `0x3C108`, `0x3C110`. Slot 28's asset name is
  at `0xA8 + 28*48`. DC4 restore reads these fields around `0x425A93` and
  `0x425B23`; PH restore does so around `0x425903..0x4259AB`.

PC import now reads these fields directly. It does not run preceding story
branches to guess the effect. PC export writes them as well. New Arisa effect
snapshots use version 9 and retain particle positions, opacity, frame indices,
RNG and fractional time. Versions 1 through 8 remain readable. Restore uses a
validated candidate before replacing the live scene.

Disabling the setting hides and pauses ambient particles while retaining their
state. Re-enabling resumes them. Script starts/stops still update effect state
while hidden. The scene is composed in the existing 1280x720 coordinate space,
then the existing Fit/Stretch presentation and inverse touch mapping apply.

## Recording review

Every decodable frame of the supplied recordings was processed. Consecutive
frame differences, full night contact sheets and selected enlarged crops were
reviewed; this is not a claim that every sakura frame was inspected at full size.

| Recording | Decoded video | Finding |
|---|---|---|
| `20261104-0911-26.8042638` | 649 frames, 648 adjacent differences | Sakura visible at the tree scene. Enlarged frames 300..315 show a pale petal moving through the crop. |
| `20261104-0950-15.6872046` | 72 frames, 71 adjacent differences, 2.4 seconds | Pale circular particles drift and fade over the night street. All three 24-frame contact sheets were inspected. |
| `20261104-0956-37.3229391` | 1 frame | No usable motion; user confirmed a recording problem. |
| `20261104-1003-10.2700580` | 40 frames, about 1.333 seconds | Game area black; user confirmed lag/recording failure. Cannot establish afternoon appearance from this file. |

`build/sakura-trigger/particle-tracks.json` contains exploratory threshold
tracking. The sakura candidate locked onto stationary background detail and is
rejected as a trajectory measurement. The night crop sequence visibly follows
the circular particle as it fades, but its threshold centroid becomes unreliable
at low opacity; neither candidate is presented as an exact engine velocity.
Use the native constants above for simulation timing. Audio/container duration
is not interchangeable with the number of captured video frames.

Native-save renders were visually inspected after import: `DC4PHDL07.dat`
restores type 11, parameter 46 at the tree scene; `DC4PHDL01.dat` restores
floating lights on the night street. `DC4PHDL08.dat` also restores lights.
The final on/off comparisons change 3,023, 3,196 and 2,973 pixels respectively
for slots 01, 07 and 08. These PNGs were regenerated from the final test's PPMs
and visually inspected after the atlas-stride correction. Particle random seeds
are not synchronized to the PC videos; pixel-identical trajectories are not claimed.

## Validation

Validated on 2026-11-04. Evidence paths below are relative to
`build/dc4-visual-effects/` unless stated otherwise.

| Check | Result and evidence |
|---|---|
| Desktop effects | PASS: `effects.log` ends with `dc4_effect_test failures=0`, using original assets from both games. |
| Native PC saves | PASS: the same log records restored effects for PH slots 01, 07 and 08; final on/off renders were visually inspected. |
| Scene Replay triggers | PASS: `dc4_asa20190429c1.mes` and `dc4_mid20190402e.mes` execute their authored triggers with the replay variable enabled. |
| Scene regression | PASS: `scene.log` ends with `Scene regressions passed`. |
| Title menu regression | PASS: `dc4-menu.log` and `dc4ph-menu.log` record title animation and On/Off checks. |
| Host preferences | PASS: 20 display checks, including legacy-key migration and new-key precedence, plus 10 screen-mode checks. |
| Android native effects | PASS: `device-effects.log` ends with `dc4_effect_test failures=0`, linked against the release APK's arm64 library. |
| Release build | PASS: `release-build.log`, arm64-v8a and armeabi-v7a. APK contents include Visual effect, its new preference field, Fit to Screen and Stretch. |
| QA build | PASS: `qa-rebuild-final.log`, 65 executed tasks. The ABI-targeted APKs are under `android/app/build/intermediates/apk/`, as identified by Gradle's APK listing redirects. |
| Android display settings | PASS: 306 assertions in `device-display-final.log`, using the current APKs verified in `qa-apk-content.log`. |
| Android screen mode | PASS: 360 assertions in `device-screen-final.log`, using the same current QA APKs. |
| Release installation | PASS: `install.log`; `installed-hash.log` matches the delivered APK; `launch.log` reports `Status: ok` on device `10DF1N05QN000H8`. |
| QA cleanup | PASS: `qa-cleanup.log` records successful removal of only `org.dc4engine.updateqa.test` and `org.dc4engine.updateqa`; `launch-after-qa.log` records the production Activity starting with `Status: ok`. |

The ABI-targeted QA build writes to `intermediates/apk/`; `outputs/apk/`
contains older debug APKs. The initial 298-assertion run used those older APKs
and is not evidence for the new toggle. The final 306-assertion run above uses
the current app and instrumentation, including the in-game Visual effect check.

The display checks exercise On/Off, Cancel, Back, font choices, keyboard
selection of a font-size row, per-game persistence and EN/ID/JP at 200% text
size. The in-game
entry test checks that a setting change queues work for the engine thread;
it does not launch a real game in the QA Activity. Native effect pause/resume
and restored particle state are tested separately by the effects suite.

Thirteen display captures are retained in `captures/display-settings-checks/`.
The EN/ID/JP Visual effect selectors and ID/JP settings lists were visually
inspected at 200% text: labels stay within their horizontal bounds, the long
settings list scrolls, and selector Cancel buttons remain visible. A temporary
font-size toast from an earlier test obscures part of the Off row in these
captures. The harness separately verifies row bounds, ellipsis and touch targets
of at least 44 dp; the screenshots do not establish unobstructed visibility
during that transient toast.

Screen-mode checks cover wide, portrait, 4:3, 16:9 and fractional viewports;
inverse touch mapping and out-of-frame rejection; composed sprite/effect/UI
markers; GPU Off/Bilinear/FXAA; Activity overlays; movie rectangles and
MediaPlayer TextureView transforms. They also exercise all four game profiles,
live changes, persistence, Cancel and keyboard activation. Six captures are in
`captures/screen-mode-checks/`. Fit/Stretch markers, Stretch overlays and ID/JP
large-text selectors were visually inspected. The selectors wrap inside their
bounds and keep Cancel visible. The overlay's 60.0 FPS label is a synthetic
fixture, not a performance measurement. These checks do not constitute an
Android playthrough or real-scene movie captures.

The native device probe uses original DC4 assets and synthetic command/save
fixtures. The actual PH save and replay-script checks above ran on desktop.
It measured software drawing rather than display FPS. Its retained output is
`build/sakura-validation/android-c6d575c336c8/result.log`.

Scope limitations: old Arisa saves that contain neither native effect fields
nor an effect snapshot cannot recover an already-running emitter until a new
script trigger occurs. Other unmapped ambient/screen effect types are outside
this change. Finite transitions must finish before saving, as before.
The older unrelated `LibraryChecks` archive harness has a known `wrong game
rejected` failure, documented in `tests/android/README.md`; it is not reported
as passing here.

## Release artifact

- APK: `build/dc4-visual-effects/Arisa32-v0.6.6-visual-effects.apk`.
- Package `org.dc4engine`, version 0.6.6, code 13.
- APK SHA-256: `84e976ca8fd1a3c6dec8a3bbe1688f9ed4261afe7df1276c869c66e37a3e0fb1`.
- Tested arm64 `libarisa32.so` SHA-256:
  `9407182b91928bfb2bbd556bcf5e91e9dd5c5f8f34e6d47f81577c90f2232c2d`.

## Antislop delivery gate

Direction: the established Arisa32 settings sheet and native radio dialogs.
ENERGY 1 / RHYTHM 1 / MOTION 1 apply to settings; the explicitly requested,
authored game animation is separate. Palette, typography and spacing are
inherited for continuity. A binary selector reflects the single requested
decision. No new artwork, icons, cards, badges or decorative motion were added.

The gate covers the changed settings and selectors on the tested device. It
does not claim a full-app accessibility audit. Each PASS below is limited to
that scope and the recorded checks.

### Hard gate

| Item | Result and evidence |
|---|---|
| R-02 | PASS: Visual effect, On, Off and localized Cancel labels contain no em dash. |
| R-03 | PASS: 306 display assertions include row bounds/ellipsis checks; inspected 200% EN/ID/JP selectors remain within their horizontal bounds. Long settings lists scroll. |
| R-17 | PASS: the selector displays persisted On/Off state, with no promotional statistics. |
| R-18 | PASS: no testimonials or invented people were added. |
| R-23 | PASS: existing settings navigation and original game atlases are reused; the user requested both effects and their control. |
| R-24 | PASS: Manage game and in-game Visual effect entries open the selector in the device harness. |
| R-25 | PASS: the contrast checker gives 14.68:1 for #E6EDF5, 5.94:1 for #8899AC and 8.08:1 for #38BDF8 against #131B26. Native white choice text also exceeds AA on this background. |
| R-26 | PASS: On/Off, Cancel, Back and persistence are exercised in the 306 device assertions; native tests separately verify visibility and pause/resume. |
| R-27 | PASS: host preferences cover missing values, legacy migration and new-key precedence; selections save and show feedback. This local selector adds no asynchronous loading operation. |
| R-28 | PASS: no FAQ was added. |
| R-32 | PASS: the shared native list control retains focus; the display harness exercises Tab/Enter on a font-size row and Back on the effect selector. Keyboard activation of On/Off itself was not separately exercised. No custom focus suppression was added. |
| R-33 | PASS: behavior is implemented in Java/JNI/C++ source; no runtime source-patching step is required. |
| R-34 | PASS: existing fixed dialog palette retained; no theme toggle introduced. |
| R-35 | PASS: release and current QA builds pass; 306 display and 360 screen assertions pass; installed release hash and successful launch are recorded above. |
| R-36 | PASS: no new security, compliance or performance claims appear in the selector; this report distinguishes simulation ticks and software drawing from display FPS. |
| R-37 | PASS: existing Arisa32 dialog direction was selected before editing; settings dials remain 1/1/1. |
| R-38 | PASS: shown values reflect actual per-game preferences; synthetic scenes and FPS labels are confined to QA and identified above. |

### Purpose gate

| Item | Result and evidence |
|---|---|
| R-01 | PASS: the existing solid dialog background is retained; no gradient or decorative glow added. |
| R-04 | PASS: the control uses text and native radio indicators to convey two exclusive states; no decorative icons added. |
| R-06 | PASS: existing dialog typography and engine caption preserve Arisa32 identity; native text follows system scaling. |
| R-07 | PASS: no background grid or patterned treatment added. |
| R-08 | PASS: no decorative arrows added. |
| R-09 | PASS: no badges added. |
| R-10 | PASS: existing opaque dialog surface retained; no glass treatment added. |
| R-12 | PASS: existing modal elevation identifies the active selector; no new shadow system added. |
| R-13 | PASS: no UI glow added. The light atlas reproduces the explicitly requested game effect. |
| R-14 | PASS: uniform radio rows allow direct comparison of On and Off; no feature cards added. |
| R-19 | PASS: no settings animation added; authored particles implement the requested game behavior and can be disabled. |
| R-22 | PASS: original sakura and EFFYOUSEI atlases are used; no generated illustration added. |

### Liveliness

| Item | Result and evidence |
|---|---|
| Dials | PASS: ENERGY 1 / RHYTHM 1 / MOTION 1 declared for settings. |
| Consistency | PASS: inspected captures retain calm native radio rows and no added motion. |
| Focal point | PASS: Visual effect or its Japanese title heads the two choices. |
| Whitespace | PASS: inherited row padding separates the controls; device assertions verify touch targets at least 44 dp. |
| Accent | PASS: existing cyan distinguishes selected radio state and Cancel action. |
| Identity | PASS: engine caption, palette and modal typography retain the established Arisa32 pattern. |
| Design Read | PASS: existing settings design supplied the direction before changes; no independent redesign was introduced. |

### Craftsmanship and quality locks

| Item | Result and evidence |
|---|---|
| C-1 | PASS: the binary selector matches one per-game decision; inherited presentation keeps it consistent with neighboring settings. |
| C-2 | PASS: both choices and dismissal actions have device coverage; the in-game entry queues the native change. |
| C-3 | PASS: the existing sakura setting is broadened to control both requested effects, with no unrelated section added. |
| C-4 | PASS: defaults, migration, both states, three languages and 200% text pass; native keyboard behavior has representative font-size selection coverage as qualified in R-32. |
| C-5 | PASS: test totals and artifact identity come from retained logs; old-save, recording and Android playthrough limits are disclosed. |
| R-05 | PASS: existing settings list and radio dialog are reused; no template page added. |
| R-11 | PASS: native dialog, row and radio shapes retained; no new pill treatment. |
| R-15 | PASS: On, Off and Cancel state their actions directly. |
| R-16 | PASS: changed setting labels contain no marketing language. |
| R-20 | PASS: the control operates actual CIRCUS game effects within Arisa32's existing settings. |
| R-21 | PASS: the existing dark palette is preserved for app consistency; no new theme direction imposed. |
| R-29 | PASS: existing neutral dialog colors and cyan accent reused. |
| R-30 | PASS: follows the app's established dialogs, with no external product imitation. |
| R-31 | PASS: palette and typography preserve identity; native rows support text scaling and touch; two radio choices expose one exclusive state. |

UI, human, mobile and copy supplements: PASS within the scope above. Native
controls retain focus and selection behavior, text scaling and scrolling; the
changed UI adds no text input, software-keyboard dependency or decorative motion.

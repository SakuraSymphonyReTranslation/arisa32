# FXAA Android 5–12 and MS Gothic size validation

Validated 2026-11-04 in E:/Games/Adv32roid.

## Delivered behavior

- Android 5–12 (API 21–32) has an EGL14/OpenGL ES 2.0 presentation path for
  Off, Bilinear and FXAA. All three use the same surface producer so live
  switching does not require reconnecting Canvas and EGL.
- Android 13+ retains the existing Canvas RuntimeShader implementation.
  The chooser exposes FXAA on every supported Android version and no longer
  describes it as Android 13+ only. Bilinear remains the default.
- The GLES shader implements the same luminance thresholds and directional
  samples as the AGSL shader, using source-pixel offsets. Each completed native
  bitmap is uploaded, including writes that reuse one Bitmap instance.
- Game frames, native effects, sprites and decoded movies use the existing
  ScreenViewport Fit/Stretch geometry. Overlays are drawn after FXAA using
  premultiplied alpha. Android MediaPlayer's separate TextureView keeps its
  existing scaling.
- Shader failure uses real Bilinear output. An EGL failure retries the context
  once; repeated failure disconnects EGL and uses Canvas until that Surface
  is recreated. Renderer shutdown releases EGL programs, textures and the
  optional overlay bitmap. GPU readback exists only in the tests.
- D.C.4 and D.C.4 Plus Harmony now allow dialogue font sizes 18, 19, 20, 21 and
  22 with MS Gothic, explicit MSGOTHIC.TTC paths, system fonts and custom fonts.
  The old fixed-size explanation was removed. The 0.6.7 release audit found
  and removed a remaining JNI launch-time override to 22; the earlier checks
  below covered the UI and scene renderer separately, not that launch path.
  Defaults remain 22; choices persist per game and apply on reopening the game.
  This does not add dialogue-size controls to the D.C.II profiles.

## Observed coverage and limits

The connected I2219 runs Android 16 / API 36. The standalone compatibility
application forces the exact production GLES renderer on this phone; this is
GLES execution on Android 16, not an Android 5–12 device test.

Official API 21 x86 and API 32 x86_64 system images were downloaded and their
published SHA-1 hashes checked. The installed emulator has no hardware
acceleration driver. Attempts with software acceleration, SwiftShader and a
host-GPU retry did not reach a booted test device. Those attempts are not
counted as successful OS coverage. Test emulator processes were stopped.

The isolated renderer passes Android lint with minSdk 21: zero errors and
three warnings (fixed QA orientation, missing QA icon, redundant SDK >=21
check). The explicit API 26 Canvas guard and nested API 33 RuntimeShader guard
remain. Lint and GLES2 execution cannot establish compatibility with every
older vendor driver. No old-device FPS, battery or memory-performance claim is
made. Actual Android 5–12 hardware testing remains unverified.

## Executed checks

| Check | Result | Evidence under build/fxaa-legacy |
|---|---|---|
| Final standalone GLES renderer, API 36 | PASS, 227 assertions | compat-api36-final.log |
| Main app filters, settings and AGSL, API 36 | PASS, 391 assertions (includes 227 GLES assertions) | graphicsOnly-api36.log |
| Fit/Stretch, touch mappings, settings and movie geometry | PASS, 360 assertions | screenOnly-api36.log |
| Font choices, effects and settings | PASS, 754 assertions | displayOnly-api36.log |
| Native scene regressions, including bundled MS Gothic at sizes 18–22 for DC4/DC4PH and both language layouts | PASS | scene-test.log |
| QA build | PASS | qa-build.log |
| Final isolated GLES build and lint | PASS | compat-final-build.log |
| Final two-ABI release build | PASS | release-final-build.log |
| Installed release launch | PASS, cold Activity launch returned Status: ok | install-release.log, launch-release.log |

The native font tests compare rendered frames across sizes, preserve the
selected size through script reloads, check bounds and verify save-state
restoration does not replace the user's size preference. The UI tests cover
all five sizes for default MS Gothic, its explicit path, system and custom
fonts, per-game persistence, Cancel/Back, keyboard activation and EN/ID/JP at
200% text. These are synthetic fixtures, not a playthrough of a user's patch.

GLES checks cover nearest pixels, bilinear interpolation, actual FXAA edge
changes, flat colors, orientation, non-power-of-two textures, same-instance
Bitmap updates, source immutability, eight fresh contexts, Fit/Stretch at
wide/portrait/fractional sizes, movie texture resizing, premultiplied overlays,
live filter changes, real window presentation, EGL recovery, Surface replacement
and persistent Canvas fallback across renderer restart.

One final QA run initially captured the window before its size transition
settled after targeting API 36. The test now presents during a bounded wait for
the expected overlay pixel, matching the production render loop, and saves each
lifecycle capture. The same pixel assertion then passed for all three lifecycle
states. The production draw path did not need a workaround for this test timing.

Captures reviewed:

- api36-final/gles-fxaa.png and gles-bilinear.png: diagonal filtering.
- api36-final/gles-window-0.png through gles-window-2.png: real Surface output.
- api36/gles-markers-fit.png and gles-markers-stretch.png: composed markers.
- frame-filter/settings-en-keyboard.png and settings-id.png: scrolling and focus.
- display-settings/en-large-text-size.png, id-large-text-size.png and
  jp-large-text-size.png: size selection and fixed dismissal action.

The size screenshots include queued toast feedback from earlier English test
selections. The dialog labels themselves use the selected language. Lists scroll
at 200% text; a capture shows only the currently visible rows.

## Installed artifact

- APK: build/fxaa-legacy/Arisa32-v0.6.6-fxaa-ms-gothic.apk
- Package: org.dc4engine
- Version: 0.6.6 / code 13 (local update, no published release)
- Minimum API: 21; target API: 36
- Native ABIs: arm64-v8a and armeabi-v7a
- Size: 13,309,252 bytes
- SHA-256: 4EFDB4E672696531CB4A4D5FB2AC3807E190225F4898E79295400598B3085E79
- APK signature verification: valid v1 and v2 signatures.
- The installed base APK was pulled back and has the identical SHA-256.
- Startup log captured in production-logcat.log; no FATAL EXCEPTION or render
  loop failure found in that captured process log.

The four isolated QA packages were uninstalled after evidence collection. The
production process remained running. Final git diff --check passed; unrelated
pre-existing workspace changes were preserved.

## Antislop delivery gate

Mode: during, explicit session choice. Existing Arisa32 settings provide the
design direction. ENERGY 1 / RHYTHM 1 / MOTION 1. This gate covers the changed
filter description and newly available font controls on the tested device,
not a full-app audit or certification of untested Android versions.

### Hard gate

| Item | Result and evidence |
|---|---|
| R-02 | PASS: changed filter descriptions contain no em dash; existing numeric size choices are reused. |
| R-03 | PASS: 754 display checks include visible row bounds; reviewed EN/ID/JP size dialogs at 200% remain within horizontal bounds and scroll vertically. |
| R-17 | PASS: controls show real filter and font preferences, with no promotional statistics. |
| R-18 | PASS: no testimonials or invented people added. |
| R-23 | PASS: existing settings navigation and assets reused for the explicitly requested controls. |
| R-24 | PASS: font and filter settings open their real selectors in device tests; no links added. |
| R-25 | PASS: contrast-check.py gives 14.68:1 for #E6EDF5, 5.94:1 for #8899AC and 8.08:1 for #38BDF8 against #131B26. Native white size labels also exceed AA. |
| R-26 | PASS: every size 18–22 and each filter is selected by the harness; Cancel/Close, Back and persistence are exercised. |
| R-27 | PASS: default selections remain valid; font choices save with reopen feedback; the filter includes fallback text and actual failure recovery is tested. |
| R-28 | PASS: no FAQ added. |
| R-32 | PASS: font Tab/Enter selection and filter keyboard navigation pass; reviewed screenshot shows cyan focus outline. |
| R-33 | PASS: runtime behavior is written in Java source; the compatibility build copies exact sources without transformations. |
| R-34 | PASS: existing fixed dialog palette retained; no theme toggle added. |
| R-35 | PASS: QA and release builds succeed; 391 filter, 360 screen and 754 display checks pass; final GLES checks pass 227 and the installed release launches. |
| R-36 | PASS: no new performance, battery or security claims; actual OS testing limits are stated above. |
| R-37 | PASS: existing Arisa32 settings direction and dials 1/1/1 retained before editing. |
| R-38 | PASS: choices reflect actual preferences; synthetic QA sources are explicitly identified. |

### Purpose gate

| Item | Result and evidence |
|---|---|
| R-01 | PASS: existing solid modal backgrounds retained; no gradient or decorative glow added. |
| R-04 | PASS: radio indicators represent mutually exclusive choices; no decorative icons added. |
| R-06 | PASS: inherited dialog typography and engine caption preserve app identity and system text scaling. |
| R-07 | PASS: no patterned background added. |
| R-08 | PASS: no decorative arrows added. |
| R-09 | PASS: no badges added. |
| R-10 | PASS: existing opaque dialog surfaces retained. |
| R-12 | PASS: existing modal elevation identifies the active selector; no shadow system added. |
| R-13 | PASS: no glow added. |
| R-14 | PASS: uniform radio rows support comparison of filter modes and font sizes; no feature cards added. |
| R-19 | PASS: no new UI motion; the controls retain native interaction feedback. |
| R-22 | PASS: no new illustrations. |

### Liveliness

| Item | Result and evidence |
|---|---|
| Dials | PASS: ENERGY 1 / RHYTHM 1 / MOTION 1. |
| Consistency | PASS: reviewed captures retain native radio lists and no added animation. |
| Focal point | PASS: selector heading and checked option identify the current decision. |
| Whitespace | PASS: inherited padding separates radio rows; display bounds and touch targets are checked. |
| Accent | PASS: cyan marks selection, keyboard focus and dismissal. |
| Identity | PASS: existing engine caption, typography and palette retain the app's settings pattern. |
| Design Read | PASS: the task extends existing Arisa32 settings, with its direction retained before changes. |

### Craftsmanship and quality locks

| Item | Result and evidence |
|---|---|
| C-1 | PASS: the existing selectors expose actual filter and font-size decisions; no decorative redesign. |
| C-2 | PASS: all changed choices and dismissal actions have device coverage. |
| C-3 | PASS: removes the obsolete fixed-size explanation and old OS limit; adds no filler section. |
| C-4 | PASS within tested scope: defaults, selection states, three languages, 200% text, scrolling and keyboard are exercised; untested old OS limits remain explicit. |
| C-5 | PASS: test counts, hashes and build results come from retained output; no old-OS or patch playthrough claimed. |
| R-05 | PASS: existing lists and selectors retained without a template page. |
| R-11 | PASS: native radio and modal shapes retained. |
| R-15 | PASS: Off, Bilinear, FXAA, numeric sizes and Cancel/Close state the choices and actions. |
| R-16 | PASS: changed descriptions contain no marketing language. |
| R-20 | PASS: settings operate Arisa32 game rendering and per-game dialogue preferences. |
| R-21 | PASS: existing dark palette retained for consistency. |
| R-29 | PASS: existing neutral colors and cyan accent reused. |
| R-30 | PASS: follows Arisa32's own dialogs. |
| R-31 | PASS: inherited palette/typography preserve identity; scrolling radio controls support comparison, touch and text scaling. |

UI, human, mobile, copy and comment supplements: PASS within this scope. New
comments explain resource ownership, bitmap uploads or alpha composition;
controls retain focus, scrolling, feedback and system text scaling.

# CG preview back button

Validated on 2026-11-05/06 with physical Android device `10DF1N05QN000H8`.

The Android button appears when the native menu is displaying an opened CG.
Touching it returns to the same gallery. It is absent on the title screen,
gallery grid and other menus. Android Back also closes an opened CG.

The button follows the existing floating-control preference. Opening a CG
reveals the controls; auto-hide hides them after four seconds. The existing
left-edge handle reveals them again without changing the CG. Swiping the
existing navigation controls left also hides the gallery button. Disabling
auto-hide keeps it visible. Keyboard focus and an active press defer the timer.

## Design direction

Use Arisa32's existing cyan text, navy navigation surface and Android typeface.
The bottom-right position separates the return action from the upper-left
settings controls. A text label makes its destination explicit. The button
uses a minimum 48dp target, wrap-content sizing and safe-area margins.
ENERGY 1 / RHYTHM 1 / MOTION 1: the CG remains the focal point; the shared
control shape and cyan accent provide continuity. No new artwork or animation.

## Results and evidence

- PASS: QA app and instrumentation APKs built with AGP 8.5.2 via a local init
  script. The repository's existing `android/build.gradle` edit was preserved.
- PASS: normal-ID debug APK built, including arm64-v8a and armeabi-v7a libraries.
- PASS: `galleryOnly` reported **502 checks** using generated DC4 and Plus Harmony
  MES/CRX fixtures. Covered title/config/music/scenes/extras/grid visibility,
  localized labels, variant taps, real button taps, duplicate close requests,
  Android Back, keyboard Tab/Enter, hide/reveal, auto-hide Off, focused controls,
  pause/resume, full-screen On/Off and Fit/Stretch.
- PASS: English, Indonesian and Japanese; text scales 1x and 2x; normal landscape
  and a 380dp container. Assertions check full visibility, no ellipsis, safe
  bounds and 48dp targets. Pixel comparisons verify the same gallery is restored
  and touching the reveal handle does not change the CG.
- PASS: `navigationOnly` reported **430 checks** for existing floating controls.
- PASS: `system_menu_test <AdvData> --menus` exited 0 for both DC4 All Age and
  DC4 Plus Harmony assets, including preview-state and return-to-gallery checks.
- PASS: `git diff --check`.

Local evidence is under `build/gallery-preview/`: build logs,
`android-gallery-checks.log`, `android-navigation-checks.log`,
`native-check.log`, and `final-captures/gallery-preview-checks/`.
Inspected captures include `aab6-en-hidden.png`, `aab6-id-preview.png` and
`abb6-jp-focus.png`. The flat gray backgrounds are generated test fixtures,
not screenshots of commercial CG artwork.

The broader sidebar suite stops at its assertion that directly changing
`bubble.x` survives `clampBubble()`. The existing FloatingNavigation code owns
the drag offset and resets that direct change; this positioning code was not
changed here. The full native replay suite also encounters its fixed
469-scene assertion with Plus Harmony's 493-scene catalogue. These suites are
not reported as passing. No baseline APK rerun or all-device compatibility
claim is made. The new `--menus` option runs menu checks independently of that
replay catalogue assumption.

Build warnings concern the existing AGP/compileSdk combination and deprecated
APIs. The final device run no longer produced the gallery Button layout loop
found during development: max width is updated only when its value changes.

## Antislop delivery gate

Scope: the new gallery control and its integration with existing navigation.

### Hard gate

- R-02 PASS: the three button labels contain no em dash.
- R-03 PASS: 380dp/2x text and full-width device bounds assertions passed.
- R-17 PASS: no product statistics were added; test counts come from runner output.
- R-18 PASS: no testimonials or people were added.
- R-23 PASS: the user requested the return action and hiding behavior; no product artwork was created.
- R-24 PASS: the return action targets the existing gallery.
- R-25 PASS: contrast checker measured cyan `#38bdf8` against `#2c3b4f` at 5.31:1. This is the rounded composite of the existing `#e514243b` surface over white. The modeled ripple composite `#2d475f` measured 4.50:1.
- R-26 PASS: injected taps return to an identical gallery frame; edge taps reveal controls without advancing the CG.
- R-27 PASS: initial/closed state is GONE; native state, loading, movie and paused checks gate visibility.
- R-28 PASS: no FAQ or filler copy was added.
- R-32 PASS: Tab/Enter and Android Back passed; the focused Japanese capture shows the cyan focus outline.
- R-33 PASS: behavior is implemented in Java/C++ source, with generated assets confined to tests.
- R-34 PASS: no theme toggle was introduced; the control uses the existing navigation drawable.
- R-35 PASS: both APK variants built; the new control and reveal path were exercised on-device and captured.
- R-36 PASS: no security, performance or customer claim was added to the product.
- R-37 PASS: the existing Arisa32 navigation style and 1/1/1 dials were selected before implementation.
- R-38 PASS: no invented product content; test fixture backgrounds are identified above.

### Purpose gate

- R-01 PASS: no new gradient or glow; navy backs readable cyan text over CGs.
- R-04 PASS: the return action uses a destination label, with no new icon.
- R-06 PASS: Android's existing button typeface preserves platform readability and user text scaling.
- R-07 PASS: no background pattern was added.
- R-08 PASS: no decorative arrow was added.
- R-09 PASS: no badge was added.
- R-10 PASS: one existing semitransparent navigation surface is reused, with no blur.
- R-12 PASS: no additional large shadow treatment was introduced.
- R-13 PASS: no glow was added.
- R-14 PASS: no cards were added.
- R-19 PASS: no new transition animation; the existing four-second hide timer serves unobstructed viewing.
- R-22 PASS: no illustration was added.

### Liveliness

- Dials PASS: ENERGY 1 / RHYTHM 1 / MOTION 1 are declared above.
- Consistency PASS: one compact return control, with no new motion or panel.
- Focal point PASS: the CG occupies the canvas; hidden-state capture contains no return overlay.
- Whitespace PASS: 12dp safe-edge margins separate the target from system edges.
- Accent PASS: cyan is reused for actionable text and focus.
- Identity PASS: the existing navy rounded navigation surface is repeated.
- Design read PASS: the existing-app direction was stated before the first edit.

### Craftsmanship and quality locks

- C-1 PASS: placement, label, sizing and color each have the purpose stated above.
- C-2 PASS: return and reveal actions passed real input tests.
- C-3 PASS: only the requested preview action was added.
- C-4 PASS: the new control passed the documented state, scale and keyboard matrix.
- C-5 PASS: results and wider-suite limitations are reported separately.
- R-05 PASS: a single contextual control preserves the existing screen composition.
- R-11 PASS: the existing 18dp navigation radius is reused.
- R-15 PASS: "Back to gallery" names the destination.
- R-16 PASS: the labels contain no marketing language.
- R-20 PASS: the control follows the existing Arisa32 overlay motif.
- R-21 PASS: navy comes from the existing app navigation style.
- R-29 PASS: no new palette colors were added.
- R-30 PASS: no external product layout was copied.
- R-31 PASS: all changed visual decisions are explained in Design direction.

## Debug APK

`build/gallery-preview/Arisa32-cg-preview-debug.apk`, 14,251,753 bytes.
SHA-256: `cb4928b9eca8c6d44a1e0d3c7d966f14fbb620495d62d21cdad1f034b3c49a45`.
This is a local debug build, not a published release.

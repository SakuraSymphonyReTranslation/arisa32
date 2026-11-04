# Screen mode validation

Validated on 2026-11-04 for the added Fit to Screen / Stretch setting.

## Behavior

- Library game settings and the in-game three-dot menu expose Screen mode.
- Fit to Screen is the default and keeps the composed 1280x720 aspect ratio.
- Stretch fills the available app surface, changing proportions. The final
  composed frame scales as a unit, including backgrounds, sprites, effects
  and game UI. Native positions and resolution do not change.
- Touch uses the inverse of the same viewport. FPS/AUTO overlays use separate
  horizontal and vertical scales; movie output follows the selected mode.
- Changes apply immediately to the current game and persist separately per game.
- Native Android dialogs and floating controls retain their normal dimensions.
  Black margins authored inside a game's frame remain; this is not cropping.

## Verification

| Check | Result and evidence |
|---|---|
| Host screen preferences | PASS: 10 assertions for defaults, persistence, isolation and malformed/missing values. |
| Host display preferences | PASS: 17 existing font/sakura preference assertions. |
| Android screen checks | PASS: 360 assertions, `build/screen-mode/device.log`. |
| Display settings regression | PASS: 298 assertions, `build/screen-mode/display-regression.log`. |
| Floating navigation regression | PASS: 431 assertions, `build/screen-mode/navigation-regression.log`. |
| QA build | PASS: `build/screen-mode/qa-build.log`. Compiled bytecode includes MediaPlayer transform and in-game entry tests. |
| Release build | PASS: `build/screen-mode/release-build.log`, arm64-v8a and armeabi-v7a. |
| Installation | PASS: `adb install -r`, `build/screen-mode/install.log`; production launch reports `Status: ok` in `launch.log`. |
| Installed identity | PASS: installed base APK SHA-256 matches the delivered APK, `build/screen-mode/installed-hash.log`. |

Device checks cover wide, portrait, 4:3, 16:9 and fractional geometry; touch
round trips and out-of-frame rejection; synthetic composed sprite/effect/UI
markers; GPU Off/Bilinear/FXAA; Activity overlays; movie rectangles and
MediaPlayer TextureView transforms; all four game profiles; immediate changes,
Cancel, persistence and focused keyboard activation. The Activity's real
SurfaceView receives touch events and its queued native coordinates are checked.

Six captures are retained in `build/screen-mode/captures/`. Japanese and
Indonesian dialogs at 200% text wrap within their bounds; Cancel remains
visible. A toast from an earlier selection overlaps later screenshots due
to rapid test switching. Marker and overlay captures were visually checked.
The optional original-title fixture was unavailable to the device test, so
these results do not claim real-scene captures or an exhaustive game playthrough.
The synthetic overlay's 60.0 FPS label is a test fixture, not a benchmark.

Native renderer code was unchanged by the screen-mode feature. Existing
unrelated native changes remain in the build. This report does not claim the
older archive `LibraryChecks` harness passed; its known archive-identity failure
is documented in `tests/android/README.md`.

## Artifact

- `build/screen-mode/Arisa32-v0.6.6-screen-mode.apk`
- Package `org.dc4engine`, version 0.6.6, code 13.
- Device `10DF1N05QN000H8`.
- SHA-256: `fc5ea0125d5869fb941927725817fd65e91130d040e63ea533eb9011cfb2fa1e`.

## Antislop delivery gate

Scope: the added setting and selector. Direction: existing Arisa32 dialogs,
palette, typography and spacing. ENERGY 1 / RHYTHM 1 / MOTION 1. Evidence below
is limited to this addition and the recorded device configuration.

| Item | Result and evidence |
|---|---|
| R-02 | PASS: new setting labels contain no em dash. |
| R-03 | PASS: device bounds/ellipsis checks and EN/ID/JP large-text captures show wrapping inside the selector. |
| R-17 | PASS: UI shows saved mode values, no promotional statistics. |
| R-18 | PASS: no testimonials or invented people. |
| R-23 | PASS: requested setting reuses existing navigation/dialogs; no visual asset generated. |
| R-24 | PASS: library and in-game entries open the selector in device tests. |
| R-25 | PASS: unchanged dialog colors: #E6EDF5 on #131B26 14.68:1, #8899AC 5.94:1, #38BDF8 8.08:1, white 17.32:1; calculations retained in prior display-settings validation. |
| R-26 | PASS: both choices and Cancel exercised; state persists and live mode changes. |
| R-27 | PASS: missing/malformed values default to Fit; selection confirms saving; no asynchronous loading state added. |
| R-28 | PASS: no FAQ. |
| R-32 | PASS: native list focus and keyboard activation exercised; dismissal leaves selection unchanged. |
| R-33 | PASS: implementation lives in Java source, with no runtime source patching. |
| R-34 | PASS: existing fixed palette retained; no theme toggle added. |
| R-35 | PASS: build, 360 screen assertions, regressions, install and launch recorded above. |
| R-36 | PASS: no security/compliance/performance claim in new UI. |
| R-37 | PASS: established Arisa32 dialog direction selected before edits. |
| R-38 | PASS: mode names reflect actual behavior and stored values; fixtures remain in tests. |
| R-01 | PASS: solid existing dialog background; no gradient or glow added. |
| R-04 | PASS: text choices without decorative icons. |
| R-06 | PASS: existing native typography supports system text scaling. |
| R-07 | PASS: no background grid. |
| R-08 | PASS: no decorative arrows. |
| R-09 | PASS: no badges added. |
| R-10 | PASS: existing opaque modal, no glass treatment. |
| R-12 | PASS: existing modal elevation only. |
| R-13 | PASS: no glow added. |
| R-14 | PASS: uniform radio rows compare two modes; no feature cards. |
| R-19 | PASS: no new settings animation. |
| R-22 | PASS: no illustrations added. |
| Dials | PASS: ENERGY 1 / RHYTHM 1 / MOTION 1 declared. |
| Dial consistency | PASS: simple native radio selection, no extra motion. |
| Focal point | PASS: setting title leads the two choices. |
| Whitespace | PASS: existing padded radio rows separate choices and touch targets. |
| Accent | PASS: existing cyan identifies selection/actions. |
| Identity | PASS: Arisa32 caption and palette retained in captures. |
| Design Read | PASS: existing settings language chosen before implementation. |
| C-1 | PASS: inherited presentation preserves consistency; radio selection matches two exclusive modes. |
| C-2 | PASS: both selections, Cancel and menu entry actions tested. |
| C-3 | PASS: only the requested mode setting added. |
| C-4 | PASS: both modes, four profiles, three languages, 200% text and keyboard selection covered; not an exhaustive device audit. |
| C-5 | PASS: test totals and installed hash come from retained output; real-scene coverage limits disclosed. |
| R-05 | PASS: existing settings list extended, no unrelated page template. |
| R-11 | PASS: existing native shapes retained. |
| R-15 | PASS: mode choices explain proportions/filling; Cancel names its action. |
| R-16 | PASS: no marketing copy. |
| R-20 | PASS: setting controls actual game rendering within Arisa32's existing interface. |
| R-21 | PASS: existing palette retained, no new theme decision. |
| R-29 | PASS: existing neutral colors and cyan accent reused. |
| R-30 | PASS: follows this app's interface, no external clone. |
| R-31 | PASS: native font/spacing supports text scaling and touch; radio rows expose mutually exclusive choices; palette preserves identity. |

UI, human, mobile and copy supplements: PASS within the selector scope above.
The single viewport comment explains the drawing/touch constraint; it is one
line, has no decoration and does not narrate the implementation. Feature logic
was implemented as the requested change, not as comment cleanup.

# DC2DM local fixes, 2026-10-31; DC4/navigation extension, 2026-11-01

This work continues the DC2DM compatibility fixes. The compatibility changes are restricted to the `dc2dm` profile or its native MES format `0x9D72` (ClassicDM). The user's later request for optional texture smoothing explicitly applies to every game; the duplicate FPS settings row is also an application-level fix. The requested DC2DM visual reference is the installed PC game and its supplied artwork, with ENERGY 1 / RHYTHM 1 / MOTION 1. No new game artwork was generated.

## Verified changes

- Date graphics honor all four native raw `0x68` origin modes and the raw `0x33` reset. The January 14 transition now occupies the lower left of the canvas.
- Dialogue keeps nominal 22-pixel cells, 10-pixel Latin advancement, and the installed translation patch's 17-pixel Latin glyph raster. Speaker names use the native encoded-name centering rule.
- Backlog uses four native rows, the original portrait atlas, saved-scene portrait variants, native control regions, and palette values from `LOG.mes`. Yuto uses portrait 15; Yoshiyuki intentionally uses the blank pink circle at portrait 17.
- Message-window FACE slots 18 and 19 load the named image from the referenced CRM archive under `GRP/FACE`, including when the same name exists under `GRP/BST`. Portraits retain their image offsets and native 248 by 247 crop at the scripted origin. Yuto's portrait and Maya's face with glasses are visible in the actual-data captures. Raw `0x4C` clearing, background preservation, and snapshot restoration have dedicated regression coverage.
- Voice preloads are silent. Actual voice commands play once, and silent dialogue no longer inherits a preceding row's replay association.
- DC2DM exposes the floating sidebar control during ordinary play. Scenario replay continues to reject sidebar access and direct Save/Load requests. Floating controls respect Android system-bar padding.
- Confirmation captions use the installed `ADVMENU_CHIP.CRX` artwork in every application language. Replay exit selects the existing `Selesai melihat?` image. The added new-game confirmation uses the existing generic `Kamu yakin?` image because the native atlas has no separate new-game caption.
- CONFIG preview uses the native 22-pixel cell, 29-pixel line spacing, translation-patch spaces, timed character reveal, and the original opacity-controlled preview panel. Desktop and Android advance the preview from their existing frame timers. The native transparency variable now maps in the correct direction, for both the preview and the DC2DM story window. At full reveal the second line ends ten blank pixel rows above the green controls in the 1280 by 720 capture.
- Global image smoothing offers Off (nearest), Bilinear (the previous behavior and default), and FXAA on Android 13 or later. The selected mode applies to the composed game bitmap, covering native menus, backlog, prompts, and story. It persists across Activity recreation. Android MediaPlayer video and application overlays retain their own rendering. FXAA falls back to Bilinear if the required hardware Canvas or shader is unavailable.
- App settings now contain one FPS Meter row, under Appearance. Its existing On/Off dialog, immediate update, and persistence remain available.

Native evidence is recorded in [backlog, voice and text notes](dc2dm-native-backlog.md), [popup notes](dc2dm-native-popups.md), [message portrait notes](dc2dm-native-faces.md), and [CONFIG preview notes](dc2dm-native-config-preview.md). Filter implementation and Android checks are recorded in [texture smoothing notes](texture-smoothing-validation.md).

The user's later DC4/DC4PH confirmation fixes and global floating-control
auto-hide request are implemented and documented in
[confirmation and navigation notes](dc4-confirmations-navigation.md). Both
DC4 profiles hide/restore their three-dot control; DC2PC and DC2DM also
hide/restore Back / Sidemenu with it. These later changes are included with the
DC2DM and graphics fixes in the final `Arisa32-DC2-DC4-fixes-debug.apk`; the
older DC2DM-only APK is superseded. The native bank-93 confirmation binding is
restricted to identified DC4/DC4PH data, independently of the DC2DM fixes.

## Validation records

The final desktop build passed all 22 CTest suites after the FACE, CONFIG timing, and transparency changes. The actual-data `dc2dm_menu_test` run completed with exit code 0 and captured the preview at 0, 1000, and 2500 milliseconds. The actual-data sprite test completed with zero failures. Logs are under `build/dc2dm-current-validation/`.

The Android sidebar instrumentation passed 89 checks after the system-bar padding correction, keyboard focus changes, and explicit DC2DM profile gating. That run predates the final native FACE and CONFIG changes; it is a focused navigation result, not a claim that the final APK repeated all 89 checks.

The final texture-filter instrumentation passed 187 checks on the I2219, Android API 36. It covers hardware shader output, software fallback, changed native-frame input, switching modes, real game samples, one FPS row with On/Off persistence, EN/ID/JP choices, 200% text, 380dp dialog width, local dialog touch dispatch, keyboard activation, Escape, the dialog Back callback, and Activity recreation. The passing run began at 2026-10-31T03:38:07Z and took 19.96 seconds. Its Back check calls the dialog callback directly; it is not a predictive/system Back gesture test. The isolated QA package was stopped afterward. The main application package was not replaced by the QA run.

Visual artifacts inspected by the primary agent:

- `build/dc2dm-check/text-final-comparison.png`: PC and portable dialogue raster comparison.
- `build/dc2dm-check/date-final-native.png`: actual January 14 date commands.
- `build/dc2dm-current-validation/backlog-final-native.bmp`: real game backlog at 1280 by 720.
- `build/dc2dm-current-validation/backlog-final-phone.bmp`: the same backlog in a 1600 by 720 window.
- `build/dc2dm-check/popup-newgame-final.png` and `popup-replay-exit-final.png`: original caption artwork rendered without loading a font.
- `build/dc2dm-voice-validation/yuto-backlog-source.png`: Yuto's original atlas crop, matching the supplied PC screenshot.
- `build/dc2dm-voice-validation/yuto-scene-after.png`, `yuto-backlog-after.png`, and `maya-scene-after.png`: original message faces, Maya's glasses, and Yuto's backlog circle. These captures predate the final opacity correction.
- `build/dc2dm-current-validation/config-preview-final/config-text-start.png`, `config-text-mid.png`, and `config-text.png`: final reveal progress, spacing, smaller glyph raster, and pink preview panel, compared with the supplied PC recording.
- `build/texture-smoothing-validation/fxaa-detail-comparison.png`: 3x nearest enlargement of Off, Bilinear, and FXAA GPU output, without additional smoothing in the comparison image.
- `build/texture-smoothing-validation/fxaa-upscale-comparison.png`: Bilinear versus FXAA on the 1920 by 1080 GPU upscale, with diagnostic crops enlarged 2x using nearest sampling.

## What the FXAA images establish

The sample game frames are rendered through the same `FrameFilter` on the I2219 GPU using `HardwareRenderer`, then read back through `ImageReader`. They are offscreen GPU outputs from recorded native game frames, not screenshots of a live game SurfaceView. Raw desktop captures used for native-layout checks do not pass through the Android filter.

For the 1071 by 602 backlog output, Bilinear versus FXAA differs at 76,840 of 644,742 RGB pixels (11.92%); the mean absolute channel difference is 0.652 on the 0 to 255 scale. For the popup, 85,962 pixels differ (13.33%), with a mean channel difference of 0.422. The primary agent independently reproduced these figures with `compare-crops.py`. These percentages measure changed pixels, not an improvement percentage.

The additional real-game upscale from 1280 by 720 to 1920 by 1080 differs at 236,300 pixels (11.40%), with a mean absolute channel difference of 0.581. The primary agent inspected this pair and its enlarged text/portrait crops. `root-pixel-comparison.json` records the independent measurements. This verifies that the filter also acts when enlarging an actual game frame.

The inspected crops show softer letter, circular portrait, and caption edges. Composition and source assets remain the same, flat areas are preserved, and some text detail becomes softer. The existing renderer outline remains visible. FXAA is not an asset upscaler, does not add missing detail, and does not guarantee that every texture will look better. No FPS, power, or battery advantage has been measured.

## Practical limits

The checked dialogue metrics match the installed PC patch settings, including `DialogueSize=-15`. Android uses a different font rasterizer; this is not a claim of identical glyph pixels for every font or patch configuration. The shared font renderer still draws a dark outline where the native backlog uses a one-pixel shadow. The native transparency direction is corrected; the existing integer-percent story configuration still rounds fractional opacity percentages. These limits do not affect the image-only popup captions or portrait selection.

Old Arisa32 snapshots did not record the omitted message portrait. Their compatibility fallback restores the native portrait origin for later FACE loads; it cannot recover an image absent from the old snapshot itself. Re-enter the scene to see the portrait immediately in that case. New snapshots retain the current portrait and its origin.

Validation covers the recorded scenes and automated regression fixtures, not a playthrough of every route. Game data, the PC executable, and the installed translation assets were not modified. The existing unrelated `android/build.gradle` change and `save-screen-test-data/` remain intact. No release was published.

## Primary-agent antislop delivery gate

Mode: during, selected by the user earlier in this session. The primary agent ran this gate after source review, independent pixel comparisons, and inspection of the final CONFIG, smoothing, and localized settings captures. Scope is the changes in this report, not a redesign or accessibility certification of all existing game artwork. The user's explicit request to preserve native PC images takes precedence over restyling those images. The app setting retains the existing Arisa32 dark panel and cyan control palette.

### Hard gate

- R-02 PASS: added product labels use ordinary punctuation; no em dash was introduced in the reviewed copy.
- R-03 PASS: final EN/ID/JP captures and 187 checks verify wrapping, reachable scrolling, and a fixed Close control at 200% text and 380dp width; CONFIG text clears its buttons.
- R-17 PASS: test and pixel counts come from saved logs and independently executed comparisons; no promotional statistics were added.
- R-18 PASS: no testimonials, invented people, or generated avatars were added.
- R-23 PASS: game portraits and caption chips are decoded original assets explicitly requested by the user; only diagnostic crops were created.
- R-24 PASS: sidebar and settings controls target existing actions; 89 navigation checks cover ordinary-play and replay restrictions.
- R-25 PASS: checked app text #e6edf5, muted text #8899ac, and focus #38bdf8 against #131b26 measure 14.68:1, 5.94:1, and 8.08:1 with the installed contrast checker; native artwork is preserved per user direction.
- R-26 PASS: filter radio actions change the renderer and preferences, FPS On/Off works, Escape and dialog Back callback dismiss; the Close button is the standard AlertDialog dismissal action.
- R-27 PASS: the new setting has no asynchronous data-loading state; unsupported GPU/OS paths retain functional Bilinear rendering and the chooser explains fallback.
- R-28 PASS: no FAQ was introduced.
- R-32 PASS: final keyboard captures show the cyan focus outline; Tab, Enter, Space, and Escape paths are exercised by the graphics harness.
- R-33 PASS: runtime changes are in Java/C++ source; the comparison script only reads captures and writes diagnostic images and metrics.
- R-34 PASS: existing theme is retained; no theme switch or alternate theme was introduced by this work.
- R-35 PASS: desktop build and 22 suites pass, actual-data captures were rendered, and the isolated Android build ran the 187-check graphics/settings sequence; the earlier sidebar run has 89 checks.
- R-36 PASS: the report makes no measured FPS, battery, or quality-percentage claim; GPU pixel changes are explicitly distinguished from improvement.
- R-37 PASS: native PC fidelity and the existing app controls provide user direction, with declared ENERGY 1 / RHYTHM 1 / MOTION 1.
- R-38 PASS: settings describe implemented behavior and supplied game data; no fictional product content was added.

### Purpose gate

- R-01 PASS: no decorative gradient or glow was added; the game gradients are authored source artwork.
- R-04 PASS: no generic icon library was introduced; portraits and native icons retain their actual game roles.
- R-06 PASS: app settings reuse Android sans-serif for readable controls; DC2DM uses the measured native font grid and installed patch metrics.
- R-07 PASS: no generated grid or background pattern was added.
- R-08 PASS: backlog arrows perform native one-row/four-row navigation; no decorative CTA arrows were added.
- R-09 PASS: no promotional badge or pill label was added.
- R-10 PASS: the new chooser uses a solid panel; no glass or blur stack was introduced.
- R-12 PASS: no custom component shadow was added; standard dialog elevation separates controls from the running game.
- R-13 PASS: a 2dp cyan focus stroke identifies keyboard focus; no glow treatment was added.
- R-14 PASS: filter alternatives use one radio group because they are mutually exclusive settings, not feature cards.
- R-19 PASS: the native CONFIG reveal demonstrates text-speed settings; app motion remains interaction feedback.
- R-22 PASS: all scene and portrait illustrations are existing game assets; no generic illustration was generated.

### Liveliness

- Dials PASS: ENERGY 1 / RHYTHM 1 / MOTION 1 is explicit and consistent with a native settings/compatibility task.
- Dial consistency PASS: settings retain predictable rows and game screens retain authored composition; animation is limited to the requested preview and existing feedback.
- Focal point PASS: the open chooser concentrates attention on the filter alternatives; CONFIG places the sample inside its original pink preview panel.
- Whitespace PASS: row padding separates touch targets, and ten blank capture rows separate preview text from the green buttons.
- Accent PASS: cyan indicates app controls/focus; no additional app accent color was introduced.
- Identity PASS: the app uses its existing dark/cyan controls, while DC2DM keeps its pink frames, original portraits, and caption artwork.
- Design read PASS: the resolved native-PC reference and existing app styling governed the work before edits, as recorded in the session and report scope.

### Craftsmanship and quality locks

- C-1 PASS: original art preserves fidelity; source-sized rendering preserves coordinates; scrollable radio rows preserve readable localized choices.
- C-2 PASS: tested filter, FPS, and sidebar actions have working state changes; unavailable replay actions remain explicitly unavailable.
- C-3 PASS: the chooser contains only scope text, actual alternatives, fallback details, and dismissal; no template sections were added.
- C-4 PASS: the graphics harness covers large text, three languages, keyboard use, software fallback, and Activity restart; modern-profile native fixtures remain unchanged.
- C-5 PASS: outputs, test counts, provenance, and practical limits are recorded; offscreen GPU evidence is not represented as a live SurfaceView screenshot.
- R-05 PASS: composition follows a single setting choice and native game screens, with no generic marketing layout.
- R-11 PASS: the dialog and focus stroke reuse restrained existing radii; no pill component system was added.
- R-15 PASS: labels identify actual actions and choices: Off, Bilinear, FXAA, Close, FPS On/Off.
- R-16 PASS: added copy describes filtering and fallback without marketing claims.
- R-20 PASS: original DC2DM assets and existing Arisa32 controls retain their established identities.
- R-21 PASS: dark panels match the existing game launcher and avoid a bright settings flash over play; this work does not introduce a new theme policy.
- R-29 PASS: newly authored UI uses the existing dark neutrals, light text, and cyan accent; native game colors remain authored artwork.
- R-30 PASS: no unrelated product UI was imitated; matching DC2DM was explicitly requested.
- R-31 PASS: color identifies controls, sans-serif supports native Android text scaling, padding supplies touch targets, scrolling retains long translations, and original artwork supplies fidelity.

### Human checks

- Text and non-text contrast PASS: measured ratios above exceed 4.5:1 for text and 3:1 for the focus/control accent on the new panel.
- Focus PASS: ID/JP keyboard screenshots show a complete visible cyan outline around the selected focus target.
- Keyboard PASS: local dialog key handling activates options with Enter/Space and dismisses with Escape; the QA does not inject keys into another app.
- States PASS: checked radio state identifies the selected option beyond color, and unavailable FXAA uses the documented Bilinear path.
- Large text PASS: three language variants pass wrapping and touch checks at 200%; scrolling exposes each complete option and Close stays visible.
- Input visibility PASS: the new chooser has no text-entry field or soft-keyboard dependency.

### Primary-agent extension gate, 2026-11-01

The primary agent reviewed the confirmation binding, lifecycle/gesture code,
both actual-data confirmation logs, the final 207-check Android result, and
the final hidden, shown, keyboard-focus and EN/ID/JP chooser captures under
`build/dc4-navigation-validation/`. This extends the gate to the current
delivery. It does not replace the stated limits of the earlier native and GPU
checks or claim a complete playthrough or accessibility certification.

- Hard gate PASS: native confirmations now share the original CONFIG policy;
  Yes/No, checkbox persistence, re-enabling and reset have actual-data checks.
  The 207 Android assertions cover all four requested profiles, shared
  visibility, restoration, cancelled hide/reveal, saved drag position, real
  timer delay, paused timers, inset handling and localized settings. The APK
  builds and both engine ABIs match the QA package. R-17/R-35/R-36 evidence is
  recorded rather than inferred from an acknowledgement or build alone.
- Layout and copy PASS (R-02/R-03/R-15/R-16/R-26): the chooser uses ordinary
  On/Off labels and actual gesture instructions. The 320dp/200% text review
  found title truncation; shorter chooser titles now render fully in all
  three languages, with selectable radio choices and visible Cancel.
- Purpose and identity PASS: existing dark/cyan app controls are retained.
  A thin edge marker supplies the requested restoration action. No generated
  art, new theme, decorative animation or unrelated interface was added.
- Focus/input PASS (R-24/R-32): the final keyboard captures show a complete
  cyan focus outline on the handle; Enter restores the controls. The test
  waits for Android's touch-mode change before checking actual focus.
  Gestures dispatch only to the QA layout and the simulated game's touch
  counter stays zero. The restoration touch target is 48 by 64dp.
- Contrast PASS for the inspected app surfaces (R-25): the setting reuses the
  previously measured light text/cyan on dark panel. The cyan marker and
  focus boundary are visible on the synthetic dark game area. Authored game
  backgrounds vary; the test does not assert a fixed contrast ratio for every
  possible game frame.
- States PASS (R-27): Off shows controls immediately; On retains the shared
  state and enables the timer. Source review confirms loading/dialog,
  accessibility touch-exploration and keyboard-focus deferral, lifecycle
  cancellation and callback disposal. TalkBack itself was not device-tested.
- Dials and craft PASS: ENERGY 1 / RHYTHM 1 / MOTION 1 remains consistent;
  the controls use no show/hide animation. Final screenshots replace the
  initial captures taken before rendering completed. Scope and provenance
  distinguish synthetic Android control tests from native actual-data tests.

Final delivery metadata, APK signature evidence, source hashes and practical
limits are linked from the confirmation/navigation report. The earlier
22-suite native regression, 89-check sidebar and 187-check graphics results
retain their original scope; only the 207 navigation checks are the final
device run for this extension.

### Primary-agent corner/handle extension gate, 2026-11-01

The primary agent reviewed the new position calculations and gesture handling,
the 431-assertion device result, the full-screen DC2DM/DC4PH corner captures,
and the moved-handle DC2DM capture under
`build/navigation-position-validation/`. This extends the delivery gate to
the new APK identified in `docs/dc4-confirmations-navigation.md`.

- Direction/purpose PASS: the user's annotated screenshots request safe-corner
  controls and a vertically movable hidden marker. Existing dark/cyan styling
  and ENERGY 1 / RHYTHM 1 / MOTION 1 are retained. No new assets, decorative
  motion, themes, or unrelated interface were introduced (R-19/R-22/R-23/R-37).
- Hard gates and layout PASS (R-03/R-24/R-35/R-36): 431 device assertions check
  four profiles, fresh safe-corner offsets despite old preferences, first and
  subsequent layouts, changed insets, widths from 280dp to full screen, safe
  vertical limits, and shared visibility. The Back control stays beneath the
  dots with an 8dp gap. The handle retains its 48x64dp touch target.
- Input and state PASS: vertical dragging stays hidden and locks its axis;
  tap/right-swipe/keyboard restore controls. Cancelled and multi-finger drags
  restore the previous height. All injected gestures are local to the QA view
  tree; the simulated-game touch counter remains zero. Existing four-second
  and paused-timer checks still pass.
- Persistence PASS: the handle has its own safe-height fraction, reloaded by
  a new controller and rescaled after viewport changes. Visible controls reset
  to the corner when a game opens; temporary dragging during play is retained.
  Source review confirms shared preferences persist the fraction, but the
  device run does not claim a process-death/reboot persistence test.
- Copy/human PASS (R-15/R-16/R-25/R-26/R-32): EN/ID/JP instructions describe
  the vertical gesture and tap/right restoration; the existing localized
  On/Off chooser and keyboard checks run at 200% text. Existing cyan controls
  and focus strokes remain visually legible on the inspected dark surfaces.
  No new colors or contrast claims about arbitrary game art are introduced.
- Craft and evidence PASS: QA and ordinary APK builds pass. Ordinary package
  identity, v1/v2 signature, both engine ABIs, absence of instrumentation/game
  assets, and source hashes are recorded in the new metadata. No C++ changes
  were made for this request; prior native results retain their original scope.
- Limits recorded: one physical I2219/API 36, synthetic game areas at several
  sizes, controller recreation rather than a full process restart. Native
  gameplay, TalkBack, and every vendor's Game Booster interception behavior
  are not claimed. The marker can be moved away from such overlays manually.

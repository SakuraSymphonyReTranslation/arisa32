# CG preview rendering and navigation

Validated on 2026-11-06. This follows [the return-button work](cg-preview-back-validation.md).

## Reference and behavior

- Android reference: `C:/Users/adamb/Downloads/cgpreview_error.mp4`.
- PC reference: `C:/Users/adamb/AppData/Local/Packages/Microsoft.ScreenSketch_8wekyb3d8bbwe/TempState/Recordings/20261105-1607-02.0336241.mp4`.
- The PC recording shows complete CGs, a gallery-to-preview transition, then reveals from black when advancing. `cg_asa.mes` connects the `cgN` labels through `cg_next`, including the next thumbnail's images. `cg_draw2` loads `MSK/MASK11A` and requests `0F 21,5`.
- Previously, images taller than 720 were drawn at their original size into a 1280x720 frame. A 1920x1080 CG lost its right and bottom portions. Navigation also stopped at the selected thumbnail's variant range and changed images instantly.
- Preview now fits the complete image inside the game's canvas, preserving aspect ratio. Portrait images have side bars. The app's existing Fit/Stretch setting still controls how that canvas fits the device.
- DC4 and Plus Harmony follow the unlocked images in their route script, skip locked entries, and wrap at the route's end. Other menu profiles retain their existing sequence selection.
- Transitions use the script's type, mask, and duration operands. Mask timing uses the existing scene renderer's 60-frame/second interpretation; exact wall-clock timing parity with the PC recording was not established.
- The native screen-effects setting controls transitions. The separate app Visual effect setting controls ambient effects and does not disable these transitions.
- A tap during a transition completes that transition. The next tap advances. A canceled pointer release does not advance. Back cancels the transition and restores the gallery that opened the preview.

## Verification

- Release native build succeeded. `system_menu_test <AdvData> --menus` passed for DC4 All Age and Plus Harmony.
- `system_menu_test <AdvData> --gallery` passed for both installed datasets. Synthetic AAB6 and ABB6 fixtures verify 1920x1080 and 2560x1440 images, a 720x1080 portrait, mask direction and intermediate pixels, gallery capture on entry, unlocked route order, skipping locked/out-of-route entries, wrapping, backward scrolling, rapid taps, canceled input, Back during animation, disabled screen effects, and mask duration at three effect-speed settings.
- Original 1920x1080 CGs from both games match decoded source pixels at sixteen locations per image, including the edges, after scaling. Captures: `build/gallery-preview/dc4-render` and `build/gallery-preview/ph-render`.
- Android debug APK and instrumentation APK built for arm64-v8a and armeabi-v7a.
- On device `10DF1N05QN000H8`, `galleryOnly` passed with **769 checks**. This includes full-image corner checks, an intermediate mask frame with the left edge visible before the right, advancement beyond the first thumbnail, portrait bars, wrapping, contextual return button, automatic/manual hide, edge reveal, Android Back, and keyboard focus/activation.
- The device matrix includes AAB6/ABB6, English/Indonesian/Japanese, normal/2x text, 380dp layout, fullscreen, and Fit/Stretch. Device captures: `build/gallery-preview/device-render-checks`.
- Native full-image/intermediate captures and Android hidden/focus captures were visually inspected. `git diff --check` passed.
- Broader pre-existing suite limitations remain documented in the earlier return-button report; this does not claim a full repository test pass.

## Design and delivery gate

Direction is the user's PC game reference, with the existing Arisa32 controls. ENERGY 1 / RHYTHM 1 / MOTION 2: the artwork remains the focal point and the native sweep indicates an image change. Containment preserves composition; side bars preserve aspect ratio; controls keep their existing navy/cyan palette, labels, spacing and type. No product artwork was created.

- R-02 PASS: no product wording changed.
- R-03 PASS: source-corner and portrait-bar checks pass; the Android size/text matrix passes.
- R-17 PASS: reported counts come from the test runner.
- R-18 PASS: no testimonials or invented people were added.
- R-23 PASS: existing game artwork and mask implement the requested behavior; generated color fixtures are test data.
- R-24 PASS: Back targets the existing gallery.
- R-25 PASS: button colors are unchanged from the contrast measurements in the return-button report.
- R-26 PASS: taps advance across thumbnails; Back and edge reveal pass on-device checks.
- R-27 PASS: existing load/closed/native-preview visibility gates remain covered; locked CGs are skipped.
- R-28 PASS: no FAQ was added.
- R-32 PASS: keyboard focus/activation and Android Back pass the device suite.
- R-33 PASS: changes are in C++/Java source; fixture generation is confined to tests.
- R-34 PASS: the existing control styling is unchanged.
- R-35 PASS: native and Android builds and recorded input checks completed successfully.
- R-36 PASS: no security or product performance claim was added.
- R-37 PASS: the user explicitly selected the PC recording as the visual reference.
- R-38 PASS: displayed product images come from game data.
- R-01/R-04/R-06/R-07/R-08/R-09 PASS: no new colors, icons, typography, patterns, arrows or badges.
- R-10/R-12/R-13/R-14 PASS: no blur, shadows, glow or cards were added.
- R-19 PASS: script-selected motion indicates CG changes and stops when the reveal completes.
- R-22 PASS: no illustration was created.
- Liveliness PASS: the full CG is the focal point; containment preserves composition; cyan controls and native mask retain the existing identities; motion matches the declared purpose.
- C-1/C-2/C-3 PASS: containment and native transition each address a reported defect; controls perform tested actions; no extra sections were added.
- C-4 PASS: sizes, input, effects settings, cancellation and hide/show states pass the scoped matrix.
- C-5 PASS: timing-parity and wider-suite limits are stated above.
- R-05/R-11/R-15/R-16/R-20/R-21/R-29/R-30/R-31 PASS: existing labels and control styling are retained; the requested PC behavior determines composition and motion.
- UI skill supplement PASS: artwork is complete, motion is finite, no decorative components or invented content were added, and the existing keyboard/mobile controls passed verification.

## Installed APK

`build/gallery-preview/Arisa32-cg-render-debug.apk`, 14,257,491 bytes, application ID `org.dc4engine`, version 0.6.8 (15).

SHA-256: `b699d369980cd9f157a4a26cca33fe93782cef8d5f02acdd84f4282b16de1641`.

Installed using `adb install -r` on `10DF1N05QN000H8`; result: `Success`. Package metadata confirms version 0.6.8 (15). This is a local debug build.

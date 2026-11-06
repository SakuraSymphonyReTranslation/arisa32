# DC4 / DC4PH CG preview resolution, 6 November 2026

The opened CG preview previously sampled the original image into the shared
1280x720 menu framebuffer with nearest-neighbour filtering. Android enlarged
that intermediate bitmap to the display viewport. Full-image fitting fixed
the earlier crop, but the intermediate downscale still discarded detail.

Asset header inspection found 425 DC4 and 960 DC4PH `ev_*.crx` images, all
1920x1080. The full source image was already being decoded; the problem was
in preview rendering, not thumbnail selection or image decoding.

## Change

`SystemMenu::draw_gallery` renders modern DC4/DC4PH previews from the decoded
source into a separate canvas sized for the physical viewport. It preserves
the image aspect ratio, uses pixel-centred bilinear sampling when resizing,
and keeps exact opaque source pixels at 1:1. The filtered base canvas is
cached by CG and output size. Gallery exit, menu changes and game loading
release that cache.

The JNI entry point copies the canvas into an Android bitmap at its actual
dimensions, respecting row stride. The Android render loop uses this bitmap
directly during CG preview, bypassing the ordinary 720p menu render. Output
is bounded to 3840x2160. Input coordinates and authored transition masks
remain in the existing 1280x720 coordinate system. Fit/Stretch uses the
existing surface renderer. The HD bitmap is released on preview exit and
render-thread shutdown.

The patch does not change source artwork, progress flags, cursor input,
Music Mode or scenario selection. Earlier navigation, transition and Back
changes remain present in this working tree.

## Verification

- Native synthetic tests passed for AAB6 and ABB6: exact one-pixel detail,
  filtered downscaling, changing output dimensions, portrait bars, transition
  mask coordinates, cache invalidation on game reload, invalid dimensions,
  inactive-preview rejection, variant/group traversal, rapid taps and Back.
- Actual assets passed for both games: three opened CGs per game match
  **every source RGB pixel** at 1920x1080. Existing 720p path checks also pass.
  This is six decoded-image comparisons, not a visual audit of all 1,385 CGs.
- Both Android ABIs built successfully: arm64-v8a and armeabi-v7a.
- The isolated Android `galleryOnly` suite passed **810 checks** on I2219,
  serial `10DF1N05QN000H8`. Matrix: DC4/DC4PH, English/Indonesian/Japanese,
  normal/2x text, 380dp layout, fullscreen, Fit/Stretch, transitions, contextual
  Back, auto/manual hide, edge reveal, keyboard activation and lifecycle.
- The actual render-loop bitmap measured 1920x1080 for Fit, 1056x594 in the
  narrow layout, and 2416x1359 for Stretch on the 2408x1080 device.
- The Fit screenshot's 100x100 test patch contains exactly 5,000 black and
  5,000 white pixels, with no intermediate colours. One-pixel detail reaches
  the physical screen without being reduced to 720p first.
- Source-image comparisons and device captures were visually inspected.
  The detailed CG comparison shows smoother hair outlines; the entire image
  remains fitted. `git diff --check` passed.

Commands, after rebuilding `system_menu_test`:

```powershell
$env:DC4_MENU_CAPTURES='build/cg-hd/dc4'
build/desktop/system_menu_test.exe G:/Games/DC4/AdvData --gallery
$env:DC4_MENU_CAPTURES='build/cg-hd/dc4ph'
build/desktop/system_menu_test.exe 'H:/Games/D.C.4 Da Capo 4 Plus Harmony/Advdata' --gallery
adb -s 10DF1N05QN000H8 shell am instrument -w -e galleryOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

Evidence is under `build/cg-hd/`: `dc4/` and `dc4ph/` contain the native
renders and detail comparisons; `device/` contains the Android screenshots.
`validation.json` records APK hashes and device measurements.

## Android delivery

Both APKs were installed successfully using `adb install -r`. The normal
package is `org.dc4engine`, version 0.6.8/code 15. The QA package is
`org.dc4engine.updateqa`, version 0.6.3/code 10. The QA launcher preferences
and library were restored to their exact pre-test bytes after the suite.
Neither application's data was cleared. Both games' `system-profile.dat`
files were byte-identical before and after installation; hashes and paths
are recorded in `profiles-before.json` and `profiles-after.json`.

| APK under `build/cg-hd/` | SHA-256 |
| --- | --- |
| `Arisa32-cg-hd-debug.apk` | `ce4c5ae613653946b6401b8f877f95ce0608897dd7404233c9ad608b7b3594bd` |
| `Arisa32-QA-cg-hd-debug.apk` | `04cd761f11c5c867e8aaee166b21f9e411f513d9f723cb3241b764eb030e5b81` |

## Scoped UI delivery gate

Existing game artwork and the PC gallery remain the design reference.
No new visual asset, wording, layout or decorative motion is introduced.
The purpose of the separate canvas is to retain original CG detail; the
existing transition still explains the change of image. Native pixel tests,
device screenshots, navigation and lifecycle checks support the result.
The existing controls remain usable in the tested language, text-scale and
viewport matrix. Hard, purpose, liveliness and craftsmanship gates pass
within this rendering-only scope.

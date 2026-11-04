# Texture smoothing on Android

The optional texture smoothing setting applies to every game profile. It is
available from the library's app settings and the in-game app settings. Changes
apply to the next displayed frame and persist in the launcher's global
preferences. English, Indonesian, and Japanese labels are included.

| Selection | Rendering | Availability |
| --- | --- | --- |
| Off | Nearest-neighbor bitmap scaling | All supported Android versions |
| Bilinear | Bilinear bitmap scaling, matching the previous renderer | Default on all supported Android versions |
| FXAA | A GPU shader detects luminance edges and samples along their direction | Android 5 or later; GLES2 on API 21–32 and hardware Canvas on API 33+ |

FXAA keeps bilinear input sampling and skips its directional filtering in flat
areas. Android 5–12 uses an EGL/OpenGL ES 2.0 surface; Android 13+ uses a Canvas
RuntimeShader. Neither path runs a full-frame CPU FXAA pass. GLES uploads each
finished native bitmap, including updates to the same Bitmap instance. Bilinear remains the default, so an unchanged installation keeps
its previous rendering behavior. FXAA requires extra GPU samples; no battery or
frame-rate improvement is claimed.

The final composed game bitmap passes through the selected filter, covering
story, backlog, native confirmation dialogs, save/load, native menus, and native
decoded movie frames. Android MediaPlayer's separate TextureView retains
Android's scaling. App controls and overlays retain their normal text rendering.
The filter softens existing edges; it does not replace the native font rasterizer
or restore detail absent from the source image.

All supported Android versions offer Off, Bilinear and FXAA. On Android 5–12,
all three selections share one GLES surface so switching filters does not switch
surface producers. GPU resources are released when the render thread stops.
Overlays are composed after FXAA with premultiplied alpha. Fit/Stretch and movie
placement share the existing ScreenViewport coordinates.

A shader failure falls back to Bilinear. An EGL presentation failure retries the
context once, then uses Canvas for the remainder of that Surface's lifetime.
Software Canvas falls back from FXAA to Bilinear. Logs record these failures;
the stored selection is retained. The API 33 RuntimeShader is still isolated
behind its own version check; the Android minimum remains API 21.

See [FXAA legacy validation](fxaa-legacy-validation.md) for the current build,
GLES tests and actual operating-system coverage. The GPU measurements below
are the earlier RuntimeShader evidence, not measurements from older Android.

## Verification

The isolated `org.dc4engine.updateqa` instrumentation supports:

```powershell
adb shell am instrument -w -e graphicsOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

`FrameFilterChecks` verifies software fallback, nearest/bilinear pixels and
`GlesFrameChecks` (GPU output, Fit/Stretch, movies, overlays and Surface lifecycle). On
Android 13 or later it also renders through `HardwareRenderer`, `RenderNode`,
and `ImageReader`, verifying real AGSL compilation, changed diagonal edge
coverage, unchanged flat regions, updates to a reused bitmap, and switching
back to Off. It exercises the real settings chooser using touch and keyboard,
checks immediate selection and persisted preferences, restarts the Activity,
and captures the three languages at 200% font size in a 380dp dialog. Touch
events pass through the chooser's decor and key events through its dialog, so
test input remains inside the isolated QA window. `requestFocusFromTouch()`
enters keyboard focus without altering the production controls. The test also
verifies one FPS Meter row and persistence of both its On and Off selections.

The smoothing chooser keeps its heading inside the scrolling content and Close
outside it. This leaves room for multi-line options at 200% font size in
landscape. Keyboard focus has a cyan outline; Enter and Space select a filter,
and Back or Escape closes the chooser.

## GPU evidence

The test device is an I2219 running Android API 36. The comparison images are
offscreen GPU readbacks from `HardwareRenderer` and `ImageReader`, using native
backlog and popup captures as inputs. They do not show a live game's SurfaceView.

At 1071 × 602, the backlog FXAA output changes 76,840 of 644,742 pixels compared
with Bilinear (11.92%). Of those, 35,346 differ by at least four levels in one
8-bit channel. The mean absolute channel difference over the whole frame is
0.652 out of 255. For the popup, 85,962 pixels change (13.33%), with a mean
absolute channel difference of 0.422. These numbers establish that the shader
affects the output; they are not perceptual image-quality scores. The effect is
concentrated around edges and can look subtle when the whole frame is fitted
into a preview.

The harness also produces a 1920 × 1080 upscale of the backlog in Bilinear and
FXAA for inspection of enlarged text and artwork. Source inputs remain
unchanged, and no filter strength was changed to amplify the comparison.

Run results and device details are recorded after execution under
`build/texture-smoothing-validation/`.

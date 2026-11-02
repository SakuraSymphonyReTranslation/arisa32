# DC4 / DC4 Plus Harmony sakura effects, 2026-11-02

## Cause and rendering

The scene dispatcher did not execute the modern `0x28` ambient emitter or
`0x38` screen sprites. Slot 22 was treated as a fullscreen mask overlay,
although `SYSTEM/EFFCHANGESCREEN1` is an atlas of large and small sakura.

`scene_dc4_effects.cpp` now uses the original decoded RGBA assets. It samples
rotated/scaled atlas cells with bilinear filtering and preserves white RGB
and source transparency. The mask, ambient emitter and finite screen sprites
have separate state. This dispatch requires a DC4/DC4PH game and Modern MES
version `0xAAB6` or `0xABB6`.

## Native evidence and timing

| Behavior | DC4 executable address | Implemented behavior |
| --- | --- | --- |
| Ambient `0x28` | `0x42AB12`, initializer `0x41C620`, update `0x41D380` | Types 1/9 use slot 28 `EFFSAKURA01`, five depth layers, authored density/wind, 28px atlas cells and 60 Hz simulation. Type 13 leaves this independent emitter running. |
| Screen sprites `0x38` | `0x42F4AA`; PH `0x42F31A` | Types 0/1 use slot 22: a 732×720 large flower and 278×256 small flowers. Rotation spans 540 degrees. Type 1 has two opposing large flowers and 52 smaller flowers. |
| Mask `0x0F(21,n)` | `0x42A574`; PH `0x42A3E4`; compositor `0x407860` / `0x407900` | Slot 21's blue channel determines the old screenshot alpha, clamped from `mask + 255 - 510*t`. The original mask is sampled at its native coordinates. |
| Normal fade `0x0F(0/1,n)` | `0x429FF8`, speed scaler `0x40E1E0` | `20*(n+1)` frames, with the native 0..11 effect-speed scaling. |

Mask/sakura duration is `n*12` frames at 60 Hz (`n*200` ms), independent of
the effect-speed scaler. Small flowers use `(n + rand()%3)*200` ms and fade
out; the large white flowers retain their opacity. Individual random paths
are not expected to match another PC session's random seed.

The default corpus sequence selects `0x38(1,5)`, `0x0F(21,5)`, then
`WAIT(5,0)` and `WAIT(254,0)`. The animation starts without inserting an
additional script wait: the existing 1000 ms wait runs concurrently, then
WAIT 254 covers any remaining small flowers, up to 1400 ms total. Faster
authored branches select parameter 2. These script conditions remain intact.

The small flowers survive normal VM resumption at the end of the main wipe.
Explicit skip or effects/animation disable still finishes the finite effect.
Ambient petals do not block dialogue, choices or automatic reading.

## Save state

New Arisa snapshots containing these effects use version 7. They retain the
mask/atlas names, ambient asset, particles, RNG and fractional tick state.
Restoration validates a separate candidate before replacing the live scene.
Loading old snapshots or a classic scenario clears stale effects. Versions
5/6 remain readable; ClassicDM ending timers still use version 6.

Native PC `.dat` ambient fields have not been mapped. Arisa's `.arisa`
companion preserves this state for new saves. An older save without effect
state cannot reconstruct an already-running emitter until the script starts
one again. Finite transitions must finish before saving.

## Reproduction

Build and run `dc4_effect_test`, or run CTest. Synthetic tests cover both
modern versions, white output, wipe polarity, exact duration boundaries,
concurrent waits, uncut small flowers, speed settings, repeated transitions,
save/load, deterministic ambient updates, toggle/stop, old snapshots and
ClassicDM isolation.

To render installed original assets, set `DC4_DATA` and `DC4PH_DATA` to the
games' AdvData directories and pass a scratch directory containing `dc4` in
its path to `dc4_effect_test`. The DC4 run also writes 43 black-background
frames at 30 samples per second and reports software render time. This is
an engine render, not a recording of the original PC executable.

After building the Android APK:

```powershell
python tests/android/run_effect_probe.py --serial DEVICE --dc4-data 'PATH/AdvData'
python tests/android/run_replay_probe.py --jdk 'PATH/TO/JDK'
```

The effect probe links the same C++ tests to `libarisa32.so` extracted from
the APK and runs in a unique `/data/local/tmp` directory. It records the
library SHA-256 and test log locally without launching or installing the app.

This change covers sakura. `0x38` type 2 (`EFFCHANGESCREEN2`, gears) and other
unmapped screen effects are outside this correction. Direct frame-by-frame
comparison with a captured PC playthrough remains unperformed.

## Validation results

- All 24 desktop CTest suites passed with installed DC4/DC4PH/DC2DM data
  (37.73 seconds), including the 1,348 map-route paths. The effect test passed
  again after the final rasterizer optimization, with both games' original
  mask and sakura assets.
- Android arm64 and armeabi-v7a APK builds succeeded. The final APK's arm64
  library passed the synthetic effect tests for both MES versions and the
  original DC4 asset tests on the connected phone.
- The same final library passed 9 JNI replay cases, 24 JNI map-route cases
  and the Java CP932 translation-space check.
- The original-asset 43-frame device sample averaged 40.51 ms software render
  time: 54.38 ms during the wipe, 8.49 ms for the remaining flowers, versus
  0.51 ms for the static scene. This is not a 60 FPS result and excludes UI,
  GPU upload and presentation. Animation clocks use elapsed milliseconds.
- APK: `build/sakura-validation/Arisa32-v0.6.3-sakura-fix-debug.apk`
  (version 0.6.3/code 10, package `org.dc4engine`).
- APK SHA-256: `e6d616d124855787b320f795d597867c1520959458aac9dac4c7b56fab962998`.
- Tested arm64 library SHA-256: `77d9682a30ce360a14f93c855d069231b978ab5c5ed2d4790283e18cb102006e`.

Logs and original-asset render frames remain under `build/sakura-validation/`;
no game assets are added to the repository or APK.

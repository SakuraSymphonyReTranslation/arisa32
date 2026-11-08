# Da Capo II Dearest Marriage: native script transitions

This audit covers the 311 `.mes` files in `H:/Games/DC2DM/AdvData/MES` and the installed PC executable `H:/Games/DC2DM/DC2DM.EXE`. It identifies the native behavior needed by the **Da Capo II Dearest Marriage profile only**. These addresses and mode numbers must not be applied to other Circus game profiles.

The latest recording contains a white mask transition and a separate brief group-sprite blend. All 306 recorded frames are covered in [the frame analysis](dc2dm-scene-transition-frame-analysis.md). The script inventory below covers more effects than that ten-second recording demonstrates; an occurrence count is not evidence that every corresponding story branch has been played on Android.

## Script inventory

The opcode is hexadecimal `0D`; its two parameters and all counts below are decimal. There are **12,522 literal `0D` calls** across the 311 files. Counts include story, replay, and system scripts and do not estimate how often a player encounters an effect. The extractor counts decoded literal parameter pairs, not dynamically computed calls.

| Mode | Calls | Second parameter: count | Native behavior |
| --- | ---: | --- | --- |
| 0 | 1,637 | 0: 1,594; 3: 29; 4: 3; 5: 11 | Dissolve from committed scene to pending scene |
| 1 | 1,051 | 0: 1,051 | Fade from black to pending scene |
| 2 | 1,630 | 0: 1,625; 3: 4; 5: 1 | Fade from committed scene to black |
| 3 | 16 | 1: 14; 2: 2 | Alternating scene flash |
| 4 | 6 | 0: 6 | Horizontal scene shake |
| 5 | 53 | 0: 53 | Refresh/synchronization; two runtime callbacks |
| 6 | 4 | 0: 4 | Vertical scene shake |
| 8 | 1 | 0: 1 | No animated runtime handler; used by `LOG.mes` at byte offset 141 |
| 9 | 1 | 0: 1 | Capture committed scene for configuration; `OPTION.mes` at 11,288 |
| 16 | 168 | 4: 8; 5: 160 | Tile-particle scene exit; selector chooses launch pattern |
| 17 | 68 | 1: 68 | Accelerating reveal from the left with a 64-pixel feather |
| 21 | 135 | 3: 76; 5: 59 | Authored grayscale-mask blend to pending scene |
| 31 | 27 | 0: 27 | Flush pending message/sprite operations |
| 32 | 7,696 | 0: 7,696 | Wait for pending sprite operations; no pixel snapshot of its own |
| 51 | 2 | 1: 1; 8: 1 | Schedule sprite effect `0x217` for selected slots |
| 52 | 27 | 1: 10; 2: 9; 3: 1; 4: 5; 6: 1; 8: 1 | Schedule sprite effect `0x218` for selected slots |

Native mode 20 uses the authored mask to fade the committed scene to black, but there are **no literal mode-20 calls** in this corpus. Native mode 40 shares the dissolve routine with an extra `0x1000` parameter bit; modes 41/42 dispatch to `0x40B3C0`. They also have no literal calls in this inventory and are outside the implemented subset described below.

The generated [inventory JSON](../build/dc2dm-scene-transition-20261108/native/inventory.json) includes example scripts and byte offsets for every parameter pair. [inventory.py](../build/dc2dm-scene-transition-20261108/native/inventory.py) invokes the repository's `build/desktop/mesdis.exe` and saves all 311 disassemblies under [native/mes](../build/dc2dm-scene-transition-20261108/native/mes).

## Scene source and destination

Native state at `0x4DA0E0` holds the pending edited scene; `0x51E448` holds the committed scene. Function `0x4271E0` renders the supplied scene state into a new bitmap. For mode 0, the destination is captured from `0x4DA0E0`, then blended over the committed scene. Mask initialization at `0x4098D0` captures the pending scene into `0x793310` and the committed scene into `0x793318`.

This distinction matters when multiple background/sprite changes precede one transition. The source must remain the previously presented scene, and the destination must include the new changes. Treating every `0D 32,0` as a mandatory new source snapshot can replace the source too early. Its raw handler at `0x42FE27` instead checks the pending-sprite flag at `0xC29140` and selects interpreter state `0x2E` if work remains.

The current port captures a scene-only source before advancing a dialogue batch and at `0D 32,0`. That is a compatibility boundary used by the port's scene model, not a claim that native mode 32 copies pixels. It covers the common authored pattern of barrier, scene edits, and transition. Native committed/pending state remains the reference when checking more complex sequences; this audit does not certify every intervening sprite command or branch against that model.

Mode 31's raw handler at `0x42E5E4` flushes pending message/sprite work. Unlike mode 30, it does not select redraw state 2 at `0x42E67B`. Neither mode 31 nor 32 specifies a visual wipe shape.

## Global dissolve and black fades

The native runtime dispatch uses index table `0x4319BC` and jump table `0x431980`. Mode 0 enters `0x43128F` and calls `0x4274D0`; mode 1 enters `0x43130E`; mode 2 enters `0x43144C`. The global blend routine is `0x416590`.

For RGB source `S`, destination `T`, and destination opacity `a` in 0..127, the native pixel result is:

```text
floor((S * (127 - a) + T * a) / 127)
```

The denominator is exactly 127. It is not a right shift by seven. The division-by-127 multiplication sequence appears at `0x4166C1` through `0x4166D0`, including multiplier `0x02040811`. Mode 1 uses black as `S`; mode 2 uses black as `T`.

Configuration bank `99[12]` maps to native variable `0x78F3FC`; see accessor `0x42930B`, selector comparison `0x42939B`, and return at `0x4293A0`. The default opacity steps at `0x49632C` are:

| `99[12]` | 0 | 1 | 2 | 3 |
| --- | ---: | ---: | ---: | ---: |
| Opacity step | 4 | 8 | 16 | 32 |

For an explicit second parameter 1..7, the steps are 32, 16, 8, 4, 2, 1, 1. A value of 8 or greater uses step 1 and an interval multiplier of `parameter - 7`. Each ordinary tick is 34 ms. These tables differ from the mask-transition speed table.

For mode 0, let `N = ceil(127 / step)`. The blocking routine increments opacity before drawing each step, waits `34 * interval_multiplier` ms for every step including the endpoint, and then performs one additional 34 ms settle. Its nominal duration is therefore `N * 34 * interval_multiplier + 34` ms.

Modes 1 and 2 use callbacks rather than the mode-0 blocking loop. Mode 1 reaches its final opacity, yields, then cleans up on the following callback: `N + 1` callbacks. Mode 2 reaches black, yields, clears scene state on the next callback, yields again, then finishes on one further callback: `N + 2` callbacks. All three modes increment opacity before their first effect draw. With that draw at time zero, progress is `min(127, (floor(elapsed / tick) + 1) * step)`, mode 1 lasts `N * tick`, and mode 2 lasts `(N + 1) * tick`. The first callback adds no elapsed interval, so callback counts must not be used directly as interval counts. Mode 0 still lasts `N * tick + 34` because its endpoint wait and fixed cleanup wait are explicit.

## Authored mask: modes 20 and 21

Mode 21 dispatches to `0x409E70`; mode 20 to `0x409D90`. Both initialize at `0x4098D0` and draw through `0x4099C0`. The mask samples the first byte of each native BGR pixel, corresponding to the blue channel after conversion to RGBA. The native decoder and bitmap row orientation determine its spatial direction; the original mask asset should be used instead of a synthetic ramp.

Let `M` be the mask value and `p` be transition progress. The weight of the outgoing scene is `a = M - p + 255`. At `a >= 255`, copy the outgoing pixel unchanged. At `a <= 0`, copy the destination unchanged. In between, mode 21 uses:

```text
(S * a + T * (255 - a)) >> 8
```

Mode 20 uses black as the destination and computes `(S * a) >> 8`. It is a feathered mask-to-black effect, not a hard threshold wipe. The endpoint copy branches preserve exact endpoint colors even though intermediate pixels use division by 256. This denominator differs from both the global dissolve and mode 17.

Progress begins at 0. Each draw uses the current progress, then adds the step and checks for a value of at least 510. Default mask steps for `99[12]` values 0..3 are **2, 4, 8, 16**, stored at `0x48A480`. Explicit parameters 1..5 use **4, 8, 12, 16, 24**; values 6 and above clamp to 24. Every loop waits 34 ms, and cleanup then waits one additional 34 ms. For `N = ceil(510 / step)`, nominal duration is `(N + 1) * 34` ms. The last loop can still contain a small outgoing contribution; cleanup installs the complete destination for the final settle tick.

The script's common parameters 3 and 5 therefore produce nominal durations of **1,496 ms** and **782 ms**. These are executable timings, distinct from the recording's approximately 1.867-second visible cover and 0.900-second visible reveal. Capture cadence, repeated frames, and rendering time prevent claiming an exact wall-clock match from those observations alone.

Native cleanup at `0x409D10` frees the captured bitmaps and mask. If the freed mask is still the pending mask, it clears `0x4DA118` and the stored mask name. A consumed mask must not leak into a later unrelated transition. When native effect conditions or the mask are unavailable, raw dispatch falls back from mode 20 to mode 2 and from mode 21 to mode 0 at `0x42E6CB` through `0x42E6F3`.

The corpus loads five authored mask assets through `2B 12`:

All five original masks are 1024 by 600 pixels, while the native scene is
1024 by 576. The native mask loop at `0x409A23` through `0x409A4A` clips
width and height to the screen globals, then keeps the original bitmap stride.
The decoder preserves CRX dimensions, and the optional loader resize uses
client-to-native screen ratios, both 1000 at the recorded client size. It does
not squeeze 600 mask rows into 576. Thus the port samples the top 576 rows
with `my = floor(y * 576 / 720)` and `mx = floor(x * 1024 / 1280)`; the last
24 source rows are unused. Stretching the full mask height would shift the
authored diagonal and radial geometry.

| Asset | Literal loads |
| --- | ---: |
| `MSK\MASK01A` | 1 |
| `MSK\MASK02B` | 106 |
| `MSK\MASK04A` | 2 |
| `MSK\MASK05A` | 20 |
| `MSK\MASK05B` | 5 |

Those 134 loads are recorded in [mask-assets.json](../build/dc2dm-scene-transition-20261108/native/mask-assets.json), generated by [audit_mask_assets.py](../build/dc2dm-scene-transition-20261108/native/audit_mask_assets.py). Slot 12 also loads `SYSTEM\OPTION_CHIP02` twice and `SYSTEM\VOICE_CHAR` once in `OPTION.mes`; a slot-12 load is not by itself evidence of a transition-mask asset.

The remaining mode-21 call is `suc65_0511_a1_otm.mes` at byte offset 9,021. It follows a white-scene dissolve at 8,968, barrier at 8,971, `BG\BG110_D_NO` at 8,974, and the slot-15 `SYSTEM\FOG` load at 9,005, with no fresh slot-12 mask load. Native mask consumption makes this call use the global-dissolve fallback. Retaining an earlier mask would apply an effect the script did not request.

## Accelerating feather reveal: mode 17

All 68 authored calls are `0D 17,1`. Initialization is `0x40A6E0`, the timed loop is `0x40A800`, and drawing is `0x418060`. The destination is a capture of the pending scene; the source is the committed scene. The normal native image is 1024 pixels wide.

For the used selector 1, set `k = 99[12] + 1` for configuration values 0..3. On visible frame `n`, starting with `n = 1`, the reveal distance is:

```text
D = min(W, 2 * k * n * (n + 1))
```

At native horizontal coordinate `x`, the destination opacity is 255 when `x < D`, `252 - 4 * (x - D)` for the next 64 pixels, and 0 beyond that feather. The blend is `floor((T * a + S * (255 - a)) / 255)`. The native multiplier `0x80808081` in `0x418060` confirms division by 255, not the mask routine's right shift.

The first loop increments the movement accumulator and subtracts its displacement before drawing (`0x40A85E` through `0x40A941`), so the first effect frame is `n = 1`, with `D = 4 * k`; it is not a stationary `n = 0` mask frame. The loop waits 34 ms per drawn frame and ends at the first `n` where `D >= W`, including a 34 ms endpoint hold. Cleanup reuses the last loop's timestamp, adding no separate settle tick. The reveal accelerates; a constant-speed wipe would not reproduce it. Selector 2 also exists natively but is unused by this script corpus and is not specified here.

## Effects identified outside the implemented subset

Mode 16 is a separate tile-particle exit, not the circular return-to-title transition. It is identified but **not implemented by this scene-transition change**. Native initialization at `0x408E00`, update at `0x409510`, and drawing at `0x409440` divide the committed snapshot into a 33-by-19 grid of cells. Normal cells are 32 by 32 native pixels; native display scaling changes their dimensions.

Only selectors 4 and 5 occur in the scripts. Their launch fields use fixed-point values scaled by 1024:

| Selector | Cell start delay | Initial horizontal field | Initial vertical field |
| --- | --- | --- | --- |
| 4 | 0 for every cell | `(32 - (rng & 63)) * 1024` | `(32 - (rng & 48)) * 1024` |
| 5 | `-2 * row - column` | `(2 - (rng & 8)) * 1024` | `((rng & 8) - 2) * 1024` |

Both set the horizontal acceleration field to zero. Initial lifetime is `16 + rng % 32`, and the opacity field starts at 16. The update includes fixed-point time, gravity, offscreen termination, and a fade during the final 16 lifetime units. These initialization fields alone are insufficient for a faithful implementation; the full update must be ported before claiming support. Selector 4 appears in system-menu exits, while selector 5 appears throughout story scripts.

Modes 51 and 52 are also identified but **not implemented by this scene-transition change**. Their second parameter is a sprite-slot bitmask: bits 1, 2, 4, and 8 select four slots. Raw handlers at `0x42E43E` through `0x42E5BB` assign effect codes `0x217` and `0x218` and set pending flag `0xC2912C`. Slot effect fields are `0xBE6FBC`, `0xBE6FE8`, `0xBE7014`, and `0xBE7040`.

The consumer at `0x4258DF` assigns auxiliary field value 9 for `0x217` and 7 for `0x218`, sets the field following the effect code to 2, and submits the effect through `0x403270`. The exact final sprite motion has not been fully decoded in this audit. Calling these generic scene fades or assuming the parameters are speed values would be incorrect. Mode 51 occurs at `sin_0326_f1.mes:7519` and `sin_0328_a1.mes:19068`; mode 52 has 27 occurrences across multiple story files.

Modes 3, 4, and 6 are flash and shake effects used by story scripts. They must not be described as UI-only operations. This audit identifies their runtime destinations (`0x4315B2`, `0x431664`, and `0x4317CE`); their exact native behavior is **not implemented by this scene-transition change**.

## Evidence and validation boundary

The native disassembly helper is [native.py](../build/dc2dm-scene-transition-20261108/native/native.py), using `pefile` and Capstone. Explicit address ranges are preferable to the full linear text dump because jump tables can cause linear disassembly to lose instruction alignment. `dispatch.json` is the **raw-opcode dispatch table**, not a transition-mode table; entries beyond the actual opcode range can run into unrelated data.

Relevant evidence files are:

- [Global dissolve](../build/dc2dm-scene-transition-20261108/native/dis-004274d0-00427910.txt), [global pixel blend](../build/dc2dm-scene-transition-20261108/native/dis-00416590-00416930.txt), and [runtime mode dispatch](../build/dc2dm-scene-transition-20261108/native/dis-00431210-004319f0.txt).
- [Mask capture and pixel loop](../build/dc2dm-scene-transition-20261108/native/dis-004098d0-00409d90.txt) and [mask timing](../build/dc2dm-scene-transition-20261108/native/dis-00409d90-0040a160.txt).
- [Mode-17 initialization and first draw](../build/dc2dm-scene-transition-20261108/native/dis-0040a6e0-0040a980.txt) and [feather pixel blend](../build/dc2dm-scene-transition-20261108/native/dis-00418060-004185c0.txt).
- [Tile initialization](../build/dc2dm-scene-transition-20261108/native/dis-00408e00-00409220.txt) and [tile selectors/update](../build/dc2dm-scene-transition-20261108/native/dis-00409220-004097e0.txt).
- [Raw sprite-effect handlers](../build/dc2dm-scene-transition-20261108/native/dis-0042e43e-0042e708.txt), [sprite-effect consumer](../build/dc2dm-scene-transition-20261108/native/dis-004256e0-00425a00.txt), and [effect submission](../build/dc2dm-scene-transition-20261108/native/dis-00403270-00403610.txt).

This document establishes the PC contract and script coverage. The associated implementation work targets modes 0/1/2, 17 selector 1, and 20/21, with profile isolation and synchronization corrections. It does not certify mode 16, sprite effects 51/52, a full-story playthrough, APK packaging, device installation, or Android runtime validation. Those results must be reported separately from this native audit.

## Verified mask artwork

All five script-used CRX masks were decoded with `build/desktop/crx_tool.exe` and visually inspected at their full **1024 × 600** dimensions. The tool reports 24-bit CRX data, compression 3, mode 0 for each. The [contact sheet](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/mask-contact-sheet.png) shows their original values without contrast normalization. Directions below use decoded PNG orientation and the audited native formula: lower blue-channel values begin changing to the destination first.

| Mask | Measured blue range | Verified shape and resulting order |
| --- | --- | --- |
| [MASK01A](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/MASK01A.png) | 0–68 | Radial gradient: dark center, brighter outer edges and corners. Change starts near the center and spreads outward. Its authored narrow range must be retained; stretching it to 0–255 changes the effect's timing across the image. |
| [MASK02B](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/MASK02B.png) | 0–255 | Curved gradient, darkest along the left edge and lower-left area and brightest at the upper-right. The front travels mainly left to right, with lower portions advancing earlier than corresponding upper portions. |
| [MASK04A](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/MASK04A.png) | 0–255 | Smooth diagonal gradient from dark upper-left to bright lower-right. Change advances from upper-left toward lower-right. |
| [MASK05A](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/MASK05A.png) | 0–253 | Dark horizontal band near the middle, grading toward bright top and bottom edges. The destination spreads from that band toward the top and bottom edges. |
| [MASK05B](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/MASK05B.png) | 2–255 | Bright horizontal band near the middle, with dark top and bottom edges. Change moves inward from top and bottom, leaving the middle until last. |

The two horizontal masks are nearly complementary, but their blue-channel sums are 255 or 256 rather than always 255; keep both authored files. They also contain a small vertical asymmetry: MASK05A's sampled top/middle/bottom values are 240/6/253, while MASK05B's are 15/249/3. MASK01A and MASK02B have identical RGB channels; the other masks differ between channels by at most one level. Sampling the native blue channel preserves those values.

These are measured asset shapes and directions implied by the native blend equation. They are not a claim that every mask occurrence has been played and captured on Android. The [source copies](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/originals), [hashes and pixel measurements](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/mask-measurements.json), and [analysis script](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/masks/analyze_masks.py) are saved beside the decoded PNGs. Every copied CRX matches its installed source SHA-256.

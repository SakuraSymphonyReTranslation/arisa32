# DC2DM CONFIG text preview

The DC2DM CONFIG preview uses the authored 22px text grid, 29px line spacing, a repeating text reveal, and the pink preview chip from `SYSTEM/OPTION_CHIP02`. The implementation is restricted to the ClassicDM menu profile. Other menu profiles retain their caption rendering and opacity mapping.

The native evidence below refers to 32-bit virtual addresses in `DC2DM.EXE`, SHA-256 `ad6acbc21d24ff78eb79722021cf00095c945f27f480c452eb98c0e2170ff880`. Its MES version is `0x009D72`.

## Caption geometry

`OPTION.mes` supplies these commands at offsets 9255 and 9326:

```text
67 (370,424,29,135)
50 "Sekarang_ditampilkan_dengan_kecepatan_tampilan_\nteks_normal."
5B (0x110C,1,0,9999)
```

Handler `0x42F33E` stores raw67 word 2 in the caption's line-spacing field. The caption renderer at `0x429893` calls `0x426910`, which selects a 22px glyph cell at `0x42693D`. The first baseline is `Y+18`; each newline adds the authored 29px spacing. The old menu renderer interpreted 29 as the font size, enlarging the text into the controls below it.

The portable preview uses the same translated-Latin handling as DC2DM dialogue: a 22px fixed grid with tracking -1 and a raster size of `22*15/19` (17px after rasterizer conversion). It replaces translation underscores with spaces and normalizes fullwidth Latin and translated punctuation. The Latin/Japanese content check preserves Japanese captions, including Japanese punctuation. Rendering happens on the native 1024x576 coordinate grid before scaling to 1280x720.

This geometry and the original pink artwork preserve the supplied native design, with ENERGY 1 / RHYTHM 1 / MOTION 1. The text reveal demonstrates the selected speed.

## Text reveal and repeat

The raw5B companion `0x110C` attaches native text effect type 12, implemented at `0x40389C`. `SystemMenu::update(elapsed_ms)` advances only animated DC2DM CONFIG captions. Rebuilding the layout recreates the caption state.

The text-speed setter at `0x424BF0` stores `11-v26`, where `v26` is bank 99 variable 26. Define `I = 11-clamp(v26,0,11)`:

| I | Initial visible count | Subsequent reveal |
| --- | --- | --- |
| 0 | 1 | All glyphs after 30ms |
| 1 to 3 | `4-I` | `4-I` glyphs every 30ms |
| 4 to 11 | 1 | One glyph every `(I-3)*17ms` |

The deterministic update begins with the native first-update batch. Native redraw scheduling can delay the visible change until the next frame. The timer uses `timeGetTime`, in milliseconds.

Newlines consume no reveal steps. At `0x426981`, the newline branch changes the drawing position and skips the glyph-record writer at `0x42699C`.

After revealing the text, the native routine continues incrementing the raw character counter every 100ms. It restarts when that counter exceeds `N+(20-v29)*5+1`, where N is the glyph count and v29 is bank 99 variable 29. If R is the raw count after the final reveal batch, the hold takes:

```text
max(1, N + (20-clamp(v29,0,20))*5 + 2 - R) * 100ms
```

The final batch can exceed N at speeds 9 and 10, shortening the hold. Direct slider edits preserve both the counter and its timer anchor; native setters change only the speed or wait globals. The portable implementation stores this state per caption so changing speed cannot hide already revealed text. A layout rebuild resets the preview.

## Preview backdrop and transparency

The backdrop commands at `OPTION.mes` offsets 367 and 376 are:

```text
5A (362,388,0,1561)
57 (560,87,140,1)
```

Sheet 140 is `12|0x80`. Handler `0x42F0B3` interprets bit 0x80 as the window-opacity flag, and `0x42F140` masks the sheet index with 0x3F. Looking up sheet 140 directly caused the missing pink backdrop.

The variable is transparency. Setter `0x424AD2` and getter `0x429390` store and return bank 99 variable 11 directly. Rendering at `0x42992B` passes `16-v11` to `0x403000`. The RGB blend at `0x416CFA` and RGBA blend at `0x416AC8` multiply the source by that value and divide by 16.

| Native v11 | Native source contribution | Preview alpha byte | SystemConfig opacity percent |
| --- | --- | --- | --- |
| 0 | 16/16 | 255 | 100 |
| 8 | 8/16 | 127 | 50 |
| 14 | 2/16 | 31 | 12 |

The reachable slider range is 0 to 14, so its most transparent setting retains one eighth of the source. The preview clamps to that range. DC2DM `SystemConfig::window_opacity` uses the same direction through the existing integer-percent interface, which rounds down fractional percentages. Other profiles keep their existing mapping.

## Validation

`dc2dm_menu_test` supplies synthetic MES, NAMES.DAT and CRX fixtures before its optional installed-data checks. With the bundled font, it verifies native text coordinates and size, underscore conversion, Japanese preservation, reveal speeds 0 and 11, intermediate batches, newline handling, hold/restart boundaries, direct slider edits, static captions, opacity endpoints, and unchanged modern caption rendering. The fresh desktop run after the runtime changes passed all 22 CTest tests.

Installed-data captures show the CONFIG preview at 0ms, 1000ms and 2500ms. The supplied sample has 59 glyphs. At the default speed 8, full reveal takes 1740ms, so 2500ms captures both lines during the hold. `DC2DM_DATA`, `DC2DM_MENU_FONT` and `DC2DM_MENU_CAPTURES` enable the installed-data run and its PPM captures.

The final installed-data run completed successfully, and all 22 CTest tests passed. The reviewed captures are in `build/dc2dm-current-validation/config-preview-final/`: `config-text-start.png`, `config-text-mid.png` and `config-text.png`. The previous rendering is recorded in `build/dc2dm-check/verified-menus/config-text.png`; the supplied PC reference is `build/dc2dm-check/config-preview/pc-01.png`.

The full capture contains both lines inside the pink preview. Its last changed text pixel is at y589; the green button row begins at y600. Comparing 0ms with 1000ms changes only the text region `(474,534)-(887,558)`. Comparing 1000ms with 2500ms changes `(462,534)-(1037,590)`; these bounds use an exclusive right and bottom edge.

## Review checks

- Hard Gate PASS: the full capture clears the buttons by ten blank rows; installed-data control checks completed successfully.
- Purpose-Gate PASS: authored coordinates, glyph metrics, text-speed behavior and the original backdrop each have native evidence above.
- Liveliness PASS: the supplied native pink frame, spacing and green control row retain the declared 1/1/1 direction.
- Craftsmanship and Quality Locks PASS: all 22 CTest tests pass; fixtures cover speed changes, repeats, Japanese text, transparency and modern-profile isolation. The three installed-data frames confirm visible progress and the final layout.

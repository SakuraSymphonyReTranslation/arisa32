# DC2DM native backlog evidence

Verified on 2026-10-31 from `H:\Games\DC2DM\DC2DM.EXE` using PE parsing and x86 disassembly. The file is the released translation patch, SHA-256 `ad6acbc21d24ff78eb79722021cf00095c945f27f480c452eb98c0e2170ff880`, 1,014,784 bytes. The original Japanese table remains in the file; added aliases live in `.bknames`. IDA was open, but no IDA MCP endpoint was available for this verification.

The renderer starts at `0x410CF0`. It uses `AdvData/GRP/SYSTEM/LOG_BACK.CRX` (1024×576) and `LOG_CHIP.CRX` (1024×596). Backlog portraits are atlas regions, not `FACE/*.CRM` images. Both images were decoded and visually checked with the repository's CRX decoder. Coordinates below use the native 1024×576 canvas.

## Speaker and portrait table

The native table at `0x48B4B0` contains 20 `(int32 id, char* name)` entries and a `(-1, null)` sentinel. Lookup at `0x4110AD..0x4113F7` compares the complete Shift-JIS name, including its length. The installed patch redirects six table references to `0x1107000`, with 52 entries and a sentinel. Original speaker matching is case-sensitive; scene-prefix comparisons below are separately case-insensitive.

| Portrait ID | Original Japanese lookup keys | Added keys in installed patch | Atlas source x,y |
|---:|---|---|---|
| 0 | 音姫 | Otome | 168,132 |
| 1 | 由夢 | Yume | 284,132 |
| 2 | 小恋 | Koko | 400,132 |
| 3 | ななか | Nanaka | 516,132 |
| 4 | 杏 | Anzu | 632,132 |
| 5 | 美夏 | Minatsu | 748,132 |
| 6 | エリカ | Erika | 168,248 |
| 7 | 渉 | Wataru | 284,248 |
| 8 | 茜 | Akane | 400,248 |
| 9 | 麻耶 | Maya | 516,248 |
| 10 | まゆき | Mayuki | 632,248 |
| 11 | 杉並 | Suginami | 748,248 |
| 12 | さくら | Sakura | 168,364 |
| 13 | 水越, 舞佳 | Mizukoshi, Maika | 284,364 |
| 14 | None | Yuki, 由姫 | 400,364 |
| 15 | None | Yuto, 勇斗 | 516,364 |
| 16 | イベール | Hiver, Yver, Yvel, Yve, Ivair, Ybel, Yvers | 632,364 |
| 17 | None | Yoshiyuki, 義之 | 748,364 |
| 18 | 立夏, 森園 | Rikka, Morizono | 168,480 |
| 19 | Otome scene variant | Same keys as ID 0 | 284,480 |
| 20 | 桜姫 | Yuuki | 400,480 |
| 21 | Yuuki scene variant | Same keys as ID 20 | 516,480 |
| 22 | Yuuki scene variant | Same keys as ID 20 | 632,480 |
| 23 | Yuuki scene variant | Same keys as ID 20 | 748,480 |
| 24 | はりまお | Harimao | 864,480 |

Every portrait is 116×116. At `0x411434`, source coordinates for IDs 0..23 are `x = 168 + (id % 6) * 116`, `y = 132 + (id / 6) * 116`. ID 24 has a separate branch at `0x411402` selecting `(864,480)`. ID 17 is a blank pink circle in the actual atlas. The Yuki and Yuto images existed in the original atlas but their name mappings were added by the translation patch. Unknown names use native text fallback at `0x411482`; they must not inherit another character's portrait.

## Scene variants

The lookup obtains the current history entry's scene through `0x410830`, then indexes 32-byte scene names at `0xBF4858`. Variants use that saved scene basename, not the currently executing script or a speaker suffix.

Otome starts at ID 0. Code at `0x411143..0x41127E` selects ID 19 for any case-insensitive first-five-character match against `suc61`, `suc65`, `suc66`, `suc68`, `suc69`, `suc71`, `suc74`, or `suc75`. For prefix `suc60`, only these exact, case-sensitive names from the table at `0x48B558` select ID 19:

- `suc60_1110_f1_otm`
- `suc60_1121_a1_otm`
- `suc60_1205_b1_otm`
- `suc60_1224_a1_otm`
- `suc60_1231_f1_otm`

Yuuki starts at ID 20. Code at `0x411283..0x4113CF` applies these rules in order, using case-insensitive prefix comparisons:

| Scene prefix | Prefix length | Portrait ID |
|---|---:|---:|
| `suc65`, `suc66`, `suc68`, `suc69` | 5 | 21 |
| `suc71` | 5 | 22 |
| `suc74_0131`, `suc74_0201`, `suc74_0203`, `suc74_0214`, `suc74_0325` | 10 | 22 |
| Remaining `suc74`, or `suc75` | 5 | 23 |
| Anything else | | 20 |

The `suc74_0201` comparison is duplicated in the executable; it does not change the result. Yuki (由姫, ID 14) and Yuuki (桜姫, IDs 20..23) are distinct.

## Four-row drawing and hit regions

At `0x410F4C`, the renderer limits visible entries to four. At `0x410F6E..0x410FB9`, it sets the newest row's y to 428 when four entries are visible, or `visibleCount * 133 - 104` for fewer entries. Each iteration subtracts 133 at `0x4115EF`. Thus the top-to-bottom row origins are `29, 162, 295, 428`, and fewer entries still begin at y=29. The native loop traverses newest to oldest.

| Element | Destination | Atlas source | Evidence |
|---|---|---|---|
| Row panel | `(200,rowY-4)`, 680×124 | `(228,0)` | `0x410FDF..0x41105D` |
| Portrait | `(240,rowY)`, 116×116 | Table above | `0x411400..0x411475` |
| Voice icon | `(208,rowY+8)`, 24×24 | `(84,120)` | `0x411568..0x411591` |
| Body text origin | `(364,rowY+12)` | Text | `0x410F9F`, `0x411599..0x4115D5` |
| Row hit region | `[200,880) × [rowY-4,rowY+116)` | None | `0x410FB9..0x410FCF` |

The selected or hovered row sets the text attribute to `0x87`; ordinary rows use `0x8F`. The same panel artwork is drawn in both branches. A row hit region receives ID `7 + reverseRowIndex`, where reverse index zero is the newest visible row. Hover records that reverse index in `0xB727A0`, so the selection remains highlighted after the pointer leaves the row.

`LOG.mes` code offset 39 executes raw `0x6E` with `(15,200,160,220)`. Its handler at `0x42F9E2` calls the palette setter at `0x418FD0`, assigning palette entry 15 the RGB color `#C8A0DC`. Text rendering masks the attribute with `0x3F` at `0x40255C`, so normal rows use this lavender fill. Selected rows use entry 7, whose RGB value in the native table at `0x4907D4` is `#FFFFFF`. Attribute bit `0x80` also draws a black copy at `(x+1,y+1)` through `0x4024E8..0x40253C`. Both attributes preserve this shadow.

Arisa32 retains the shared font renderer's one-pixel black outline, using the cardinal neighbors satisfying `dx*dx + dy*dy <= 1`. This differs from the native separate black copy at `(x+1,y+1)`; matching the row colors and nominal text placement does not make the glyph edges identical. The shared outline was left unchanged to preserve other game profiles.

When the speaker name is empty, the renderer subtracts 8 from rowY before placing voice/text, so narration starts at y=`rowY+4`. With an unknown nonempty name, code at `0x411482..0x41153D` uses its encoded byte length `N` to set `x = (N >= 16 ? 304 : 288) - 4*N`. It prepends `@s`, stored at `0x46AA78`, and draws at the text origin `(x,rowY+12)` with the row's current color. Matched portraits do not invoke this fallback; normal body text remains at x=364.

The `@s` parser at `0x425C6A..0x425C6E` sets nominal font size 16. For horizontal text, `0x425D52..0x425D6F` centers this size within the normal 22-pixel cell by adding 3 to its y coordinate. The fallback name's nominal cell top is therefore `rowY+15`. Native tracking remains 1, giving ASCII advance 7 and fullwidth advance 15. The caller restores nominal size 22 at `0x411549` before drawing the body. The installed DLL's smaller glyph coverage is documented below.

## Controls

`0x410C40` registers a half-open destination rectangle and ID. `0x410C80` selects normal or hover source coordinates; `0x402DE0` accepts `(layer,dstX1,dstY1,dstX2,dstY2,srcX,srcY,imageSlot)`. The backlog uses image slot 10 for this atlas.

| ID | Artwork | Destination x,y | Size | Normal source | Hover source |
|---:|---|---|---|---|---|
| 0 | Read from here | 28,66 | 160×48 | 0,204 | 0,252 |
| 1 | Large up arrow | 120,136 | 72×40 | 156,0 | 156,40 |
| 2 | Large down arrow | 120,512 | 72×40 | 84,0 | 84,40 |
| 3 | Small up arrow | 944,88 | 40×24 | 908,0 | 908,24 |
| 4 | Small down arrow | 944,444 | 40×24 | 948,0 | 948,24 |
| 5 | Scrollbar hit region | 944,114 | 40×330 | None | None |
| 6 | Exit | 936,504 | 84×68 | 0,0 | 0,68 |

The large-arrow draw right edge is x=191, while its registered hit region extends to x=192. Their nominal atlas cells are 72 pixels wide; the native draw clips one column. The small arrows and scrollbar are drawn only when more than four entries exist. Large up is drawn when the native current index is at least four; large down is drawn when it is less than `count-1`.

Scrollbar track drawing at `0x410DDA..0x410DFB` uses destination `(948,112)`, 32×332, source `(988,0)`. Thumb drawing at `0x410E00..0x410E19` uses destination `(948,112+offset)`, 32×32, source `(908,72)`, where `offset = (currentHistoryIndex-3)*300/(count-4)` with native integer division.

The scrollbar click handler at `0x4127FE..0x41283A` uses a different ratio from the draw code. Within the registered track region, it computes `topIndex = min((mouseY-114)*(count-1)/330, count-4)` with integer division, then sets `currentHistoryIndex = topIndex+3`. For example, at `count=10` and native y=279, the resulting top index is 4. Inverting the thumb's 300-pixel draw travel instead gives 3, so using the draw ratio for clicks changes native navigation.

Clicking **Read from here** resumes from the top visible row. The click handler at `0x412663..0x412686` recognizes ID 0 and first sets the selected reverse index to `min(totalCount-1,3)`. The resume path at `0x41291E` then uses `currentHistoryIndex-selectedReverseIndex`; it may move backward to a resumable checkpoint after native confirmation. Keyboard resume events `0x0B` and `0x11` enter that path without resetting the selected row. Clicking a normal history row replays its voice through `0x411F70`; it does not itself resume the scene.

## Patch boundaries and reproducibility

The installed `.bknames` section changes name lookup data and six absolute table operands. The separate `.lnpitch` section hooks `0x42A105`; it changes ordinary horizontal body line pitch from 32 to 24 only for layer 10 with `a4 == 0`. It does not change portrait coordinates, row spacing, glyph size, vertical text, or ruby offsets.

The existing local research package is `H:\Games\DC2DM\DC2DMMesTextTool_GUI and Patch\DC2DMPatch a\tools\backlog_fix`. Its `patch_backlog.py`, `patch_yoshiyuki.py`, `patch_yuto_yuki.py`, `patch_line_pitch.py`, and native Unicorn tests document the patch stages. These files were read, not executed as patch operations. The original input hash recorded by that package is `377fa93b408b829b5e44198f441a9ef2805c0881d374899641e320e076c19d0c`.

Read-only verification can use Python `pefile.PE` to obtain bytes at `VA - ImageBase`, then Capstone x86 32-bit disassembly. The important regions are `0x410C40..0x41162E` for drawing, lookup, and geometry; `0x48B4B0..0x48B558` for the original table; `0x48B558` for Otome's exact scene names; and `0x1107000..0x11071A8` for the installed table. No EXE, IDA database, or game asset was changed during this research.

## Voice commands and history association

The raw-opcode dispatcher table at `0x430EF8` distinguishes three voice operations:

| Raw opcode | Handler | Native behavior |
|---|---|---|
| `0x35` | `0x4301E2` | Copy the resource to pending name `0xBE8058` and replay name `0xBE7FB4`, then play once through `0x4367A0`. |
| `0x36` | `0x4302A8` | Store the pending name, request preload through `0x436940`, and enter VM state `0x36`. |
| `0x37` | `0x4302F0` | Request preload only. Preserve the current playback and replay name. |

VM state `0x36` resolves through byte table `0x432E64` and jump table `0x432DFC` to `0x432869`, which calls `0x42B8F0`. This routine waits for the pending presentation/loading work, copies the pending resource to the replay name at `0x42B9D0`, then calls `0x4367A0` once. The native playback call uses DirectSound flags zero, so voice playback does not loop. Arisa32 decodes synchronously at `0x35`/`0x36`; it can acknowledge `0x37` without an asynchronous preload queue.

The installed MES corpus contains 109 `0x35`, 15,565 `0x36`, and 33,656 `0x37` commands. `mar_0114_a1.mes` illustrates why preload must remain silent: code offsets 5720, 5773, and 5866 preload `MAR_0114_A1_OTM006` before Yoshiyuki's “Hup...”, narration, and “Selamat pagi, Oto-nee”. Offset 5979 actually plays that resource for Otome's reply. The following narration repeatedly preloads `OTM007` at 6073, 6250, and 6357 before playback at 6512. Treating every preload as playback restarts the next character's voice on unrelated lines.

Native input advancement clears replay name `0xBE7FB4` at `0x42C167`. Every pair of consecutive dialogue commands in the installed corpus has raw `0x06` between them. Arisa32 returns to its host at a dialogue command and consumes that following wait on the next advance, so clearing the retained replay name/blob at raw `0x06` keeps unvoiced lines from inheriting the preceding voice. This does not stop or restart the mixer channel. History capture retains the voice name before advancing.

`tests/audio_test.cpp` checks silent preloading, uninterrupted playback across repeated preloads, one-shot `0x35`/`0x36`, explicit replay, and clearing the association on advancement in both live and headless audio modes.

## Native text metrics

Normal dialogue sets nominal glyph size `0xBE3F90` to 22 at `0x431C77`; backlog does the same at `0x41109D`. The glyph writer at `0x402109..0x40211D` advances ASCII by `floor(fontSize/2)-tracking`, and fullwidth text uses `fontSize-tracking` at `0x40223C..0x402248`. The installed default tracking value at `0x49629C` is 1, giving a 10-pixel Latin cell and a 21-pixel fullwidth advance at nominal size 22.

The font name at `0x490E98` is `ＭＳ ゴシック` (MS Gothic). Startup requests the cached size-22 font with `CreateFontA(height=44,width=22)` at `0x41E8EE..0x41E8F7`. `0x40EA6E` selects this cached font for nominal size 22; the glyph rasterizer at `0x40EC79..0x40EC92` halves both raster coordinates into a 22×22 output cell. The installed DLL changes the requested font before this rasterization, as described below.

Speaker drawing at `0x42BB7D..0x42BB9E` also uses nominal size 22. Its x position is `nameOriginX + (nameColumns*22 - encodedByteLength*11)/2`, with native integer division, while y is the name layout's y coordinate. The body and speaker origins come from the MES window records loaded at `0x42A130`; body fields are copied at `0x42A16F..0x42A1AD`, and speaker fields at `0x42A1C3..0x42A1EB`.

The installed `AdvData/MES/start.mes` establishes the following window records. Offsets are relative to the decoded command stream after the header. Each listed raw `0x67` is immediately followed by an empty raw `0x50` string; raw `0x21` marks the window definition boundaries.

| Code offset | Raw command | Parameters | Meaning |
|---:|---|---|---|
| 1984 | `0x67` | `137,0,0,0` | Body descriptor |
| 1995 | `0x67` | `312,456,32,135` | Body origin `(312,456)`, line pitch 32, attribute `0x87` |
| 2006 | `0x21` | `1,0` | Body definition boundary |
| 2012 | `0x67` | `137,5,0,0` | Speaker descriptor, five nominal columns |
| 2023 | `0x67` | `264,424,32,135` | Speaker origin `(264,424)`, line pitch 32, attribute `0x87` |
| 2034 | `0x21` | `2,0` | Speaker definition boundary |

The speaker column count was also verified in the running game at `0x496304`, with value 5. The native centering formula therefore places the nine-byte name `Yoshiyuki` at x=269 and the five-byte name `Otome` at x=291. Use the encoded name length before underscore or other display normalization. Body y=456 and speaker y=424 are cell tops, not glyph baselines.

### Installed DLL font adjustment

The running game loads `H:\Games\DC2DM\DC2DMPatch.dll`, 219,648 bytes, SHA-256 `906fff024d2b82b5952bf2a73d80fb0b0b2e287e3cf2691d6e42d613ac6b2eb8`, at image base `0x10000000`. Its implementation differs from the newer `src/dc2dmpatch.cpp` alongside the research tools. The installed binary, rather than that source file, was used to establish these metrics.

The CreateFontA hook at `0x10003CF0` calls the helper at `0x10009030`. For Shift-JIS fonts with absolute requested height at least 16, when the dialogue override is enabled and nonzero, the helper computes `trunc(abs(requestedHeight) * -DialogueSize / 19.0)` and preserves the requested height's sign. The hook sets font width to zero for this adjusted request. The denominator is the float 19 at `0x10027FB8`.

The installed INI sets `DialogueSize=-15` and `DisableDialogueFont=0`. Both loaded values were confirmed read-only in the running process at `0x10033AD8` and `0x10033B04`. Thus the original 44-pixel font request becomes a positive-height 34-pixel font, which the game downsamples by two. Its live text metrics at `0xB69A08` were height 34, ascent 29, descent 5, and average character width 17. The resulting glyph bitmap occupies about a 17-pixel cell while the script's nominal size stays 22 and Latin advancement stays 10. This explains the smaller visible PC letter height without changing the spacing between letters.

The size-16 font used by unknown backlog speakers has a separate cached handle at `0xB828B4`, selected at `0x40EA9E..0x40EAA8`. Startup creates it with height 32 and width 16 at `0x41E8CA..0x41E8CE`. Under the same installed DLL settings, the requested height becomes `trunc(32*15/19) = 25` before the native rasterizer halves its coordinates. Its coverage therefore occupies roughly a 12 to 13-pixel cell, while nominal size 16, tracking, and name placement remain unchanged. Arisa32's integer raster-size equivalent is 12 for this patched Latin text, compared with 17 for normal nominal-size-22 text. The exact glyph shape and baseline depend on the font used; the native placement and the smaller raster scale are separate from the nominal advance.

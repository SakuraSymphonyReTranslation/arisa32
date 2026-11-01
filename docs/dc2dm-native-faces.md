# DC2DM textbox portrait evidence

Verified on 2026-10-31 against the installed `H:\Games\DC2DM\DC2DM.EXE`, SHA-256 `ad6acbc21d24ff78eb79722021cf00095c945f27f480c452eb98c0e2170ff880`. These portraits belong to the ordinary dialogue window. Backlog portraits use `LOG_CHIP.CRX` and a separate speaker table documented in `dc2dm-native-backlog.md`.

## Resource loading and lifecycle

Raw `0x2B` loads the face into resource slot 18 and an optional accessory into slot 19. The loader at `0x407570` calls `0x4051F0`, which splits a comma reference into entry and archive names. For example, `FACE\YTO_0C_001,FACE\YTO_C` selects entry `YTO_0C_001` from `GRP/FACE/YTO_C.CRM`. A successful load is available to the next textbox composition without an additional show flag. The raw `0x2B` branch at `0x42FA98..0x42FAB9` ignores an empty string or exact `"-"` for these slots.

Raw `0x4C` at `0x42FB6B` has these behaviors:

| String | Behavior |
|---|---|
| Empty or `"-"` | Clear both resource names at `0xBE4480`/`0xBE44B0`, handles at `0x4DA130`/`0x4DA134`, and render objects at `0x4962E8`/`0x4962EC`. |
| `"."` | Preserve the loaded face and accessory. |

The background handler at `0x42EAAE` calls body cleanup `0x428CA0`, which clears body/accessory slots 2 through 9 but preserves face slots 18 and 19. Full scenario reset `0x428CE0` clears the resource range through slot 20. Arisa32 preserves face resources during background changes and script transfers, while a fresh runner starts empty.

## Placement and composition

The textbox renderer at `0x42A090..0x42A0E8` draws slot 18 first and slot 19 second, both at layer 10/type 2. Slot 19 is drawn only when slot 18 has a valid handle. Both use a source rectangle beginning at `(0,0)`, sized **248×247**, and a shared destination origin read from `0xBE6EEC` and `0xBE6EF0`.

Raw `0x6C` at `0x42FDC5` updates the origin from its first two signed 16-bit words only when its fourth word is zero. The parameter reader at `0x424850..0x424860` sign-extends the high byte before combining the low byte. The dispatcher stores the third word at `[esp+0x10]` and the fourth at `[esp+0x14]`; the handler tests the latter. The only raw `0x6C` in the installed MES corpus is `start.mes` code offset 3730, with `(0,320,0,0)`.

Yuto's `YTO_0C_001` image decodes to a **158×192** crop with registration offset **(51,64)** inside an extended CRX canvas of 248×256. The CRM container's outer 1024×850 dimensions are not the face canvas. With the script origin `(0,320)`, the crop begins at native `(51,384)` and is clipped at y=567 by the 247-pixel portrait window. Maya's glasses use a separate 87×30 crop at `(87,121)`, demonstrating why the accessory must keep its own registration offset.

Arisa32 uses its existing comma-reference CRM decoder and retains those crop offsets. Message-face records resolve only under `GRP/FACE`: many `GRP/BST` archives contain entries with identical names but larger body artwork. For example, Maya's `MYA_0O_012` is 203×233 at `(33,23)` in FACE, but 321×630 at `(347,170)` in BST; the latter lies outside the 248-pixel portrait width. It composes the two images with straight alpha into a transparent 248×247 canvas, then applies the ordinary 1.25 screen scale. The portrait is drawn with the visible textbox, outside the camera-transformed body layers. Hiding the textbox also hides its portrait.

## State and profile boundaries

ClassicDM uses sprite records 0 through 7 for body/accessory pairs; its otherwise unused records 8 and 9 now hold message face slots 18 and 19. The existing snapshot format stores their resources and origin, so new saves round-trip without a format change. For older ClassicDM snapshots where both records are empty at `(0,0)`, restoration reads only the raw `0x6C` placement commands from `start.mes` and the saved scenario prefix. This recovers the placement for subsequent face loads without replaying flow, audio, or variable assignments. It does not reconstruct an omitted portrait already displayed when an old save was written.

The load, lifecycle, background-preservation and draw changes are gated by the ClassicDM profile. DC2PC, classic DC2, and modern profiles keep their prior sprite behavior. DC2DM native PC-save import/export remains unsupported; these checks concern Arisa32 snapshots.

## Validation

`tests/dc2dm_sprite_test.cpp` covers accessory-only suppression, immediate face visibility, slot 19 above slot 18, registration offsets, the 247-pixel crop, textbox hiding, background preservation, raw `0x4C` clear/no-op behavior, signed raw `0x6C` placement and its fourth-word mode guard. Same-named body fixtures verify that portraits load from `GRP/FACE`. It also covers a populated snapshot round-trip and placement recovery for the empty records written before FACE support.

The user-reported dialogue is in `sin_1931_b1.mes`: code offset 8065 loads `FACE\YTO_0C_001,FACE\YTO_C`, offset 8094 contains raw `0x4C "."`, offset 8138 plays `VOICE\SIN_1931_B1\SIN_1931_B1_YTO002`, offset 8176 names `Yuto`, and offset 8182 contains `「Selamat_ya！」`. Native marker 38 corresponds to the desktop capture tool's zero-based `--line 36`.

The before/after capture command uses the installed assets and bundled font:

```powershell
& build/desktop/arisa32_desktop.exe 'H:\Games\DC2DM\AdvData' --scene sin_1931_b1.mes --line 36 --shot 'E:\Games\Adv32roid\build\dc2dm-voice-validation\yuto-scene-after.bmp' --font 'E:\Games\Adv32roid\android\app\src\main\assets\fonts\MSGOTHIC.TTC'
```

The final FACE build passed `dc2dm_sprite_test` with `DC2DM_DATA=H:\Games\DC2DM\AdvData`, including the real Nanaka four-body scene at marker 65, with zero failures. The Yuto ordinary-textbox and backlog captures are `build/dc2dm-voice-validation/yuto-scene-after.png` and `yuto-backlog-after.png`. A final-build capture of the preceding Maya dialogue (`--line 35`) is `build/dc2dm-voice-validation/maya-scene-after.png`; visual inspection confirms the face and glasses align in the textbox. Its log confirms the 203×233 FACE image and 87×30 accessory both use origin `(0,320)` instead of loading the same-named BST artwork.

Native assets, the executable, and the IDA database were not modified.

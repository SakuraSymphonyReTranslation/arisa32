# DC2DM confirmation artwork

DC2DM stores the popup frame, Yes/No states and captions in `AdvData/GRP/SYSTEM/ADVMENU_CHIP.CRX`. The installed atlas already contains the Indonesian captions. The resource resolver may also select an installed overlay. UI language must not replace these image captions with font-rendered text.

`start.mes` command offsets 2043..2279 define a 440×224 panel and 366×62 caption regions at panel position `(37,45)`. Raw `0x59` calls in `main.mes`, `advmenu.mes`, `OPTION.mes`, `SAVE.mes`, `LOAD.mes` and `sce_mar.mes` place the panel at `(292,160)` in the native 1024×576 canvas. Caption placement is therefore `(329,205)`.

| Native prompt index | Atlas source | Installed caption | Used by |
| --- | --- | --- | --- |
| 0 | `(1,229)` | Akhiri Permainan | Quit |
| 3 | `(1,433)` | Kembali ke menu utama? | Title |
| 4 | `(373,229)` | Muat data? | Load and quick load |
| 5 | `(373,297)` | Simpan data? | Save, overwrite and quick save |
| 6 | `(373,365)` | Kamu yakin? | Scene replay and exact backlog resume |
| 7 | `(373,433)` | Kamu yakin ingin reset ini? | Reset settings |
| 8 | `(373,501)` | Kamu tidak bisa kembali / Kembali ke pilihan awal | Native fallback to an earlier resumable checkpoint |
| 10 | `(1,569)` | Selesai melihat? | Replay exit |

`sce_mar.mes` offset 26310 invokes prompt 6 before starting the selected scene. `advmenu.mes` offset 1266 invokes prompt 10 from `kaiso_ret`; accepting it transfers through `SC_END` to the scene list. Save and load use prompt 5 and 4 at their respective offset 2482. Settings reset uses prompt 7 at `OPTION.mes` offset 23271.

The native backlog resume handler selects prompt 8 at EXE address `0x412964` when the requested row requires returning to an earlier checkpoint; exact resume selects prompt 6 at `0x4129A3`. Arisa32 restores retained scene snapshots, so its exact jump uses prompt 6.

Arisa32's added new-game confirmation also uses the existing generic prompt 6. The native title script does not contain a separate new-game caption. The `movie01` through `movie03` confirmation routes belong to DC4; DC2DM's `music.mes` has no such movie dialog.

`tests/confirm_dialog_test.cpp` verifies the atlas coordinates, panel position and button bounds for all confirmation kinds in Japanese, English and Indonesian without loading a font. This catches accidental fallback to generated caption text.

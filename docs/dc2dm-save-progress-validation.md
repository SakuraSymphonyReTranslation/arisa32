# DC2DM save and progress validation

Date: 2026-11-07. These changes follow v0.6.9 and are not a published release.

## Behavior

D.C. II Dearest Marriage uses its native title-progress flags, including when importing `system.dat`. The original `main.mes` chooses `TITLE_BACK03`, `TITLE_BACK13`, or `TITLE_BACK23`. Loading an older slot preserves completed global progress. Imported music, CG, and scene catalogue flags survive an Arisa32 profile restart.

The inspected PC and phone `system.dat` copies both contain title flags 1 and 2 set to one. The older phone `system-profile.dat` contains zero for both. Earlier builds rejected DC2DM `system.dat`; the new import reconciles these flags before displaying the menu. No original save was edited to obtain this result.

| Purpose | Native file |
| --- | --- |
| Quick save | `DC2DM01.dat` |
| Automatic checkpoint | `DC2DM02.dat` |
| PC latest-save copy | `DC2DM03.dat` |
| Regular slots 1–100 | `DC2DM04.dat`–`DC2DM103.dat` |

New slots use the native 16,352-byte record and a checksum-linked `.dat.arisa` companion. The companion retains Arisa32's additional state and full dialogue history. Loading the native record alone reconstructs the current checkpoint, dialogue or choice, artwork, and supported audio state. Existing portable `.dc4sav` files remain supported; normal slot lookup prefers the native file when both formats occupy the same slot.

Save/Load rows use the scenario title separately from the save timestamp. The bundled table covers 279 MAR/SIN/SUC scripts. Android also reads the game's executable so translated titles can override the bundled Japanese titles. The inspected translation resolves `MAR_0114_A1` to `Awal semester terakhir sekolah`.

## Verification

- Desktop CTest: 27/27 passed, including native dialogue/choice/checkpoint restoration, malformed save rejection, paired-save handling, slot numbering, menu controls, and regressions for other supported games.
- Java profile checks: 330 passed, including all 279 titles from the inspected translated executable, mixed-case native paths, archive companions, and embedded timestamps.
- Real DC2DM assets: three title stages produce different frames, survive profile restart, and preserve progress when restoring an older scene. Catalogue bits remain available after restart.
- Android arm64 JNI probe on the connected iQOO: 12/12 cases passed. Six cases cover the three title stages and separate-process restarts; five cover PC slots `01`, `02`, `03`, `04`, and `06`; one covers scenario labels and confirmed quick/auto/latest/regular row actions.
- Each Android slot case imports a PC save, writes native and companion files, loads the pair, removes the companion, reloads the native file, compares dialogue/speaker/artwork, and advances the story.
- Save/Load captures show the translated scenario below the embedded timestamp. Long titles end in an ellipsis inside the original row; empty slots keep the original placeholder.
- After the complete device run, all 25 original files matched the pre-test SHA-256 manifest: ten PC files and fifteen phone files, including the existing portable save and global profile.

Desktop real-data checks can be repeated with `DC2DM_DATA`, `DC2DM_SAVE_DATA`, `DC2DM_MENU_FONT`, and `DC2DM_MENU_CAPTURES` set before running `scene_test` and `dc2dm_menu_test`. `DC2DM_SAVE_DATA` must point to a copy of the PC save directory.

The device probe is `tests/android/run_dc2dm_save_probe.py`. It runs the APK's arm64 JNI library in `app_process`, reads the device's game assets, and writes only to a unique `/data/local/tmp/arisa32-dc2dm-save-*` directory. Its isolated `AdvData` links only the six asset entries needed by the engine. This prevents automatic native-profile discovery from importing the player's completed progress into every test stage. Each restart case removes its staged `system.dat` first, leaving only `system-profile.dat` to restore progress. The probe waits for load transitions and advances empty checkpoints before inspecting dialogue, then uses the Save/Load screen's own confirmation path for row selections.

The probe does not install the APK or use the player's save directory as its output. It tests JNI rendering and input routing, not the complete Android Activity lifecycle. Logs and captures are pulled to `build/dc2dm-save-validation/`.

### Tested artifact and evidence

- APK: `android/app/build/outputs/apk/debug/app-debug.apk`, 15,365,489 bytes; debug build retaining version 0.6.9.
- APK SHA-256: `7567d8f759d50b2840f09ba1bd7d5940a868ccae8cf4d9fed58290ca2729fd5d`.
- Packaged arm64 `libarisa32.so` SHA-256: `3dcb4dc07aa047147e5c68adf60dbdaf6f82595694e21d6d3ab77a5624d8554e`.
- Complete Android run: [result.log](../build/dc2dm-save-validation/ef4255cbce32/result.log).
- Original-file verification: [original-preservation.json](../build/dc2dm-save-validation/ef4255cbce32/original-preservation.json).
- Android captures: [load rows](../build/dc2dm-save-validation/ef4255cbce32/captures/load-titles.png), [save rows](../build/dc2dm-save-validation/ef4255cbce32/captures/save-titles.png), [title stage 0](../build/dc2dm-save-validation/ef4255cbce32/captures/title-stage-0.png), [stage 1](../build/dc2dm-save-validation/ef4255cbce32/captures/title-stage-1.png), [stage 2](../build/dc2dm-save-validation/ef4255cbce32/captures/title-stage-2.png).
- Desktop evidence: `build/desktop/Testing/Temporary/LastTest.log`, completed 2026-11-07 16:50, with 27 passed tests. The separate `LastTestsFailed.log` is stale.

Evidence under `build/` is local and ignored by Git. The reproduction command is in [the Android test guide](../tests/android/README.md#dc2dm-save-and-progress-probe).

### Delivery gate for the changed UI

Direction: preserve the supplied game's title and Save/Load artwork. ENERGY 1 / RHYTHM 1 / MOTION 1 applies to the host changes; the original authored backgrounds and transitions remain game assets. The scope is progress-selected backgrounds, metadata text, and the affected save actions. Unchanged game artwork is not being certified for accessibility or redesigned.

| Check | Result and evidence |
| --- | --- |
| R-02, text punctuation | PASS: added scenario text comes from the game's title table; host wording adds no decorative dash. |
| R-03, containment | PASS: inspected both 1280x720 Save/Load captures; long titles are clipped with an ellipsis and remain within their rows. |
| R-17, statistics | PASS: no statistics added to the interface; test counts above come from recorded runs. |
| R-18, testimonials | PASS: no testimonials or invented people added. |
| R-23, assets | PASS: existing game sheets and title backgrounds are used; no generated visual assets. |
| R-24, navigation targets | PASS: no links added; affected quick/auto/latest/regular actions resolve in the Android probe. |
| R-25, metadata contrast | PASS: scenario and timestamp text use white glyphs with a black outline in `draw_text`, a 21:1 foreground/outline pair. Original artwork labels are outside this change. |
| R-26, controls | PASS: all four affected load actions and regular save selection return the expected action after confirmation. |
| R-27, states | PASS: occupied and empty rows appear in the captures; transition-aware loads complete; malformed records and missing artwork fallback pass desktop tests. |
| R-28, FAQ | PASS: no FAQ added. |
| R-32, keyboard path | PASS: existing desktop numeric slot keys use `visible_slot_action`, whose special and regular mappings pass `save_screen_test`; no new focusable element is introduced. Physical keyboard input on Android was not retested. |
| R-33, implementation | PASS: runtime behavior is implemented in repository C++/Java sources; the probe does not patch the APK. |
| R-34, themes | PASS: no theme toggle or new theme introduced. |
| R-35, execution | PASS: debug APK built; its JNI library ran all 12 cases, including the affected row actions. Full Activity interaction remains outside this probe. |
| R-36, claims | PASS: measured results and native-PC/Activity limits are stated separately. |
| R-37, direction | PASS: original DC2DM artwork supplies the direction; no new visual concept generated. |
| R-38, content | PASS: scenario titles and timestamps are read from the supplied executable and save records. |
| R-01, gradients | PASS: none added; the original pink game artwork is retained for compatibility. |
| R-04, icons | PASS: no generic icon set or decorative emoji added. |
| R-06, type | PASS: existing game's font path and metadata renderer retained. |
| R-07, background pattern | PASS: no host pattern added; backgrounds come from the original title scripts. |
| R-08, arrows | PASS: no new arrows; native page controls remain intact. |
| R-09, badges | PASS: no host badge introduced; native Quick/Auto/New labels identify real save roles. |
| R-10, glass | PASS: no glass effect added. |
| R-12, shadows | PASS: no host shadow added; existing sheets retained. |
| R-13, glow | PASS: no glow added. |
| R-14, repeated rows | PASS: equal rows represent equal save slots and preserve the native slot ordering. |
| R-19, motion | PASS: no decorative motion added; load transitions are exercised before checking restored state. |
| R-22, illustration | PASS: no stock or generated illustration added. |
| Dials | PASS: 1/1/1 declared for the host changes, with no new animation or composition. |
| Dial consistency | PASS: captures retain the original fixed menu and repeated save rows. |
| Focal point | PASS: title logo/menu and occupied save rows remain the existing screen focal points. |
| Whitespace | PASS: timestamp and scenario retain distinct lines in each row. |
| Accent | PASS: native selection colors retained; no second host accent introduced. |
| Identity | PASS: native sakura artwork and save sheets remain visible in the captures. |
| Design read | PASS: the supplied PC/game reference is the retained direction. |
| C-1, intent | PASS: every visible change either selects progress artwork or identifies the saved scenario. |
| C-2, functionality | PASS: affected save action/confirmation routes pass the device probe. |
| C-3, content | PASS: no template section introduced. |
| C-4, resilience | PASS: tested stage restart, native-only load, empty rows, malformed records, and controller mappings; broader device/keyboard coverage is not claimed. |
| C-5, evidence | PASS: logs, captures, hashes, and explicit limits accompany the results. |
| R-05, layout | PASS: original game layout retained. |
| R-11, shapes | PASS: no new component shape or radius introduced. |
| R-15, actions | PASS: native Save/Load and slot roles remain concrete actions. |
| R-16, copy | PASS: no marketing text added. |
| R-20, identity | PASS: original DC2DM title logo and sheets retained. |
| R-21, dark mode | PASS: no theme change introduced. |
| R-29, palette | PASS: no host palette added. |
| R-30, originality | PASS: no unrelated product template introduced. |
| R-31, reasons | PASS: artwork selection follows progress; scenario/date separation identifies what and when the player saved. |

## Limits

- Native PC executable acceptance of exported slots has not been directly tested. No successful PC application test is claimed. Native-only reload tests exercise Arisa32's codec.
- Global Android progress is written to `system-profile.dat`. This change imports `system.dat` but does not export a reciprocal PC global profile.
- New writes target the selected regular slot, quick slot, or script autosave. They do not refresh a separate `DC2DM03.dat` copy. The latest-load picker also considers regular saves.
- Native-only export begins a fresh backlog. Keep `.dat.arisa` with `.dat` to retain full Arisa32 history and additional playback state.
- Tests cover the supplied saves and scripted fixtures. They do not establish compatibility with every DC2DM patch or every possible saved scene. Support remains beta.

## Bahasa Indonesia

Menu utama kini membaca progres asli DC2DM. Judul skenario ditampilkan pada baris Save/Load, sementara tanggal tetap menjadi informasi waktu simpan. Slot baru memakai pasangan `.dat` dan `.dat.arisa`; save PC dapat dimuat tanpa pendampingnya.

Progres global Arisa32 tetap disimpan dalam `system-profile.dat`. Ekspor balik progres global ke `system.dat` dan pemuatan hasil ekspor slot dengan executable PC belum diverifikasi. Simpan kedua berkas slot agar riwayat lengkap dan keadaan tambahan Arisa32 tetap tersedia.

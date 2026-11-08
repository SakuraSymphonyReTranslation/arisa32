# Bundled Pretendard JP Bold

The requested `default.ttf` is **Pretendard JP Bold**, according to its embedded name table. The family is `Pretendard JP`, the style is `Bold`, and the version string is `Version 1.301;FEAKit 1.0`. Its OS/2 weight class is 700.

The source was copied read-only from the user's Android device at `/sdcard/Kirikiroid/Da Capo 5 Sweet Happiness/default.ttf`. The bundled file is byte-identical; only its filename changes to `PretendardJP-Bold.ttf`.

| Property | Value |
|---|---|
| Size | 3,834,112 bytes |
| SHA256 | `2386cac04bdbe169c8243e55eac43d59c5d634a3776acf3865c406c99d7c259e` |
| Copyright in font | Copyright © 2022 Kil Hyung-jin |
| License in font | SIL Open Font License, Version 1.1 |
| Font asset | `android/app/src/main/assets/fonts/PretendardJP-Bold.ttf` |
| Included notices | `android/app/src/main/assets/licenses/PretendardJP-Bold-NOTICE.txt` and `Pretendard-OFL.txt` |

The license text and upstream notices come from the [Pretendard project license](https://raw.githubusercontent.com/orioncactus/pretendard/main/LICENSE). The font retains its original name, metadata, outlines, and copyright records. Local inspection evidence is in `build/bundled-font-20261108/font-metadata.json` and `font-properties.json`.

## Picker behavior

The dialogue font picker lists `Pretendard JP Bold` after MS Gothic and System default. It extracts the bundled font into the app's `fonts/bundled` directory so an imported font with the same filename cannot replace it. The existing picker layout, colors, and controls provide the visual direction; the new row uses the actual typeface name to distinguish it from an arbitrary source filename.

An imported copy with identical bytes, including the user's `default.ttf`, shares this single named row. Opening or cancelling the picker preserves the imported file's saved path. Choosing the named row explicitly switches that game to the bundled copy. Other imported files remain listed, and MS Gothic remains the default for a game without a font selection. Each game's font setting remains independent.

## Validation entry point

`FontSelectionChecks` exercises the picker in the isolated `org.dc4engine.updateqa` application. It covers duplicate naming, imported-path preservation, Cancel, selecting each font source, persistence, another game's unchanged preference, Android typeface loading, engine font-path resolution, keyboard selection, and the existing English, Indonesian, and Japanese dialogs at a 2.0 text scale.

After building and installing the isolated QA app and its instrumentation APK, run:

```text
adb shell am instrument -w -r -e fontOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

## Device result, 2026-11-08

All 32 checks passed on the iQOO Z9x 5G. Raw instrumentation completed with `INSTRUMENTATION_CODE: -1`; see `build/bundled-font-20261108/font-qa-instrumentation.log` and `font-qa-result.json`. The QA build completed successfully. No production package preferences or game saves were changed by this font fixture.

The keyboard fixture sends Tab to leave touch mode, explicitly focuses the list at row 0, then sends Down, Down, Enter. All three languages recorded `windowFocus=true`, `listFocus=true`, `touchMode=false`, and `selected=0` before navigation. Back dismisses the reopened picker in the persistence checks; Cancel is tested separately. The fixture keeps its own window awake during screenshot capture and waits for queued selection toasts to clear without changing system settings.

The final `captures/en-font-picker.png`, `captures/id-font-picker.png`, and `captures/jp-font-picker.png` were visually inspected: the new name is fully visible, without a toast overlay or ellipsis, at a 2.0 text scale. The existing scrollable list accommodates the longer localized labels; its selected font row remains at least 44dp high. These screenshots and the per-language keyboard logs are under `build/bundled-font-20261108/`.

The picker retains the existing palette. The focused-row screenshot samples white text (`#FFFFFF`) on `#303740`, giving a 12.03:1 contrast ratio for that pair. This checks the new row's observed text/background combination, not every state or a full accessibility audit.

`qa-artifacts.json` records the QA APK hashes and verifies that all bundled fonts, notices, and arm64 native libraries matched the menu/font candidate `ed89d209...`. The final APK later received a DC2DM native read-history fix. Its DEX files, fonts, and licenses remain byte-identical to that candidate, as recorded in `build/dc2dm-title-selector-20261108/final-apk-change-identity.json`. The [combined validation report](dc2dm-menu-font-validation.md) records the installed APK and broader game checks.

# Arisa32

[Bahasa Indonesia](README.md) | **English**

Arisa32 is a C++17 reimplementation of the CIRCUS adv32 visual novel runtime for the Da Capo and Dal Segno series. It runs on Windows with SDL2 and on Android with NativeActivity + JNI, reading MES scripts, CRX/CRM artwork, and XPCM audio directly from an installed game.

**Latest release: [v0.7.2](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.2)**. This release adds beta support for D.C. II To You, including menus, voices, date-change artwork, and PC-format saves with scenario titles. It also fixes DC2DM backlog touch selection and text readability. Read the [release notes](docs/releases/v0.7.2/en.md) for details and cross-save limitations. Version history is available in the [changelog](CHANGELOG.md).

> [!IMPORTANT]
> Large Language Models (LLMs) generated all source code, system architecture, adv32 MES opcode reverse engineering, asset decoders, Android/JNI interfaces, and documentation in this repository.
>
> The project is named Arisa32, replacing the former internal name `dc4engine`. The Android package retains the `org.dc4engine` identifier to preserve save data and application paths.

> [!NOTE]
> This repository does not include game assets. You need a legitimate copy of a supported game. Arisa32 reads scripts, artwork, and audio from `AdvData` without modifying those assets. It can write save data in the game directory; see [save data](#save-data).

## Supported games

Arisa32 recognizes 21 official CIRCUS adv32 titles from the Da Capo and Dal Segno series through script and asset detection:

### Da Capo 4 series

- `D.C.4 ~Da Capo 4~`
- `D.C.4 ~Da Capo 4~ Plus Harmony` (a separate title with dedicated script route detection)

### Da Capo III series

- `D.C. III ~Da Capo III~`
- `D.C. III R ~Da Capo III R~`
- `D.C. III P.P. ~Da Capo III Platinum Partner~`
- `D.C. III With You ~Da Capo III With You~`
- `D.C. III Dream Days ~Da Capo III Dream Days~`

### Da Capo II series

- `D.C. II ~Da Capo II~`
- `D.C. II Spring Celebration ~Da Capo II Spring Celebration~`
- `D.C. II P.C. ~Da Capo II Plus Communication~`
- `D.C. II To You ~Da Capo II To You~`
- `D.C. II Fall in Love ~Da Capo II Fall in Love~`
- `D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~`

According to user testing, D.C. II Dearest Marriage is fully playable through to the end. Support includes PC saves, progress import, rock-paper-scissors, Reversi, menus, choices, voices, and the CG gallery. Version 0.7.1 fixes story selector and minigame transitions, preserves read history when importing PC progress, and adjusts text layout for Pretendard JP Bold. See the [DC2DM 0.7.1 notes](docs/releases/v0.7.1/en.md).

### Original Da Capo series

- `D.C. ~Da Capo~`
- `D.C.P.C. ~Da Capo Plus Communication~`
- `D.C. White Season ~Da Capo White Season~`
- `D.C. Summer Vacation ~Da Capo Summer Vacation~`
- `D.C.A.S. ~Da Capo After Seasons~`
- `D.C. Dream X'mas ~Da Capo Dream X'mas~`

### Dal Segno series

- `D.S. -Dal Segno-`
- `D.S. i.F. -Dal Segno- in Future`

Standard, Limited, Deluxe, and Download editions are grouped under their main title. Games that use another engine, such as D.C.4 Fortunate Departures, are not supported.

## Platforms and features

### Windows

- 1280×720 rendering with letterboxing to preserve the aspect ratio.
- Text rendering through Windows fonts, using MS Gothic by default with a TrueType font override.
- BGM, sound effects, and character voice playback from XPCM audio.
- A Save/Load system with 100 slots, quicksave, and a dialogue backlog.
- Title screen, settings, gallery, and scene selector.

### Android

- A persistent game library with automatic detection for the 21 titles above.
- Direct reading from MicroSD and USB OTG storage (`/storage/XXXX-XXXX`) without copying all game files to internal storage.
- Bundled `MSGOTHIC.TTC` for Japanese text and an optional Pretendard JP Bold font, with options for system fonts and custom TTF/OTF/TTC fonts.
- Game removal from the library with an option to delete copied files, plus duplicate entry detection.
- An adv32/CIRCUS-themed `ArisaUi` interface with an Arisa crest icon.
- ZIP save archive import and export.
- Full-screen touch controls, dialogue backlog, and dialogue skipping.
- Android API 36 target, with support for Android 5.0 / API 21 and later. The release and debug APKs each contain `arm64-v8a` and `armeabi-v7a`, including support for 16 KB memory pages on ARM64.

## Prepare the game folders

Keep the original game files and translation patch in separate folders inside the game directory:

| Contents | Folder |
| --- | --- |
| Raw / original game files without a patch | `AdvData` |
| An Indonesian translation patch, if used | `id_data` |
| An English translation patch, if used | `eng_data` |

For the original version, use `AdvData` on its own. To use a patch, keep the original files in `AdvData` and place one translation pack in an `id_data` and `eng_data` folder beside it. Preserve the patch's subfolder and file structure.

On Android:

1. Add the `AdvData` folder to the game library.
2. To use a patch, open the game settings and choose **Import language pack**. Select **Bahasa Indonesia** or **English** to match the patch, choose the text encoding specified by its author, then select the `id_data` or  `eng_data` folder.
3. Select the language to play under **Game language**. The original version uses the Japanese option; each language has separate save data.

Arisa32 detects the name `id_data` as an Indonesian patch and and `eng_data` as English patch, If that folder contains an English patch, register it through **Import language pack** with **English** selected so it is available as English.

## Build and run on Windows

Install Visual Studio or Visual Studio Build Tools with **Desktop development with C++** and **C++ CMake tools for Windows**, then run these commands from the repository directory:

```bat
build.bat
build\desktop\arisa32_desktop.exe "D:\Games\DC4\AdvData"
```

`build.bat` detects Visual Studio and creates a Release x64 build in `build/desktop`. SDL2 headers and import libraries are included in `third_party/sdl2`; the build copies `SDL2.dll` beside `arisa32_desktop.exe`.

You can also set the game location through the `DC4_ADVDATA` environment variable:

```bat
set "DC4_ADVDATA=D:\Games\DC4\AdvData"
build\desktop\arisa32_desktop.exe
```

If you omit the location, the application searches for `DC4/AdvData` near the working directory and executable. For D.C.4, the default opening scene is `dc4_kyo20190204a1.mes`.

## Build for Android

To build a debug APK on Linux, macOS, or Windows through Git Bash / MSYS2:

```bash
bash android/build_apk.sh
```

The script creates `android/Arisa32-debug.apk` from the same Gradle project used by Android Studio. Install:

- Android SDK with the `android-36` platform and the Build Tools required by Gradle.
- Android NDK `27.3.13750724`.
- CMake `3.22.1`.
- JDK 17 or later.

To build a release APK, run this command from the `android` directory:

```bash
./gradlew assembleRelease
```

The build produces a universal APK containing ARM64 and ARMv7. In PowerShell, use `.\gradlew.bat` instead of `./gradlew`. See the [Android guide](android/README.md) for build details, storage setup, and installation through ADB.

Prebuilt APKs are available on [GitHub Releases](https://github.com/SakuraSymphonyReTranslation/arisa32/releases):

| v0.7.2 APK | Minimum Android | Use |
| --- | --- | --- |
| `Arisa32-v0.7.2.apk` | 5.0 / API 21 | Release build for normal use |
| `Arisa32-v0.7.2-debug.apk` | 5.0 / API 21 | Debug build for testing and diagnosis |

Both APKs are universal, each containing ARM64 and ARMv7. `SHA256SUMS.txt` contains checksums for both APKs.

The in-app updater downloads the universal release package. Install updates without uninstalling the app to retain existing data.

## Desktop reading controls

| Input | Action |
| --- | --- |
| Left click, Space, Enter | Finish the typewriter text, then advance the dialogue |
| Right click, H, OFF/Close button | Hide or show the message window |
| A, AUTO button | Advance automatically after text and voice finish |
| S, SKIP button; hold Ctrl | Skip dialogue |
| V, VOICE button | Replay the current voice line |
| F5, QUICK SAVE button | Create a quicksave |
| F9, QUICK LOAD button | Load the quicksave |
| SAVE / LOAD buttons | Open the 100-slot Save/Load screen |
| 1-9 / 0 in Save/Load | Select slots 1-10 on the current page |
| Left/Right, Page Up/Down in Save/Load | Change page, wrapping across 10 pages |
| Tab in Save/Load | Switch between Save and Load screens |
| B, BACKLOG button, mouse wheel up | Open the dialogue backlog |
| Mouse wheel, arrow keys, Page Up/Down | Scroll the backlog |
| Click a choice or press 1-9 | Select an option |
| F11, Alt+Enter | Toggle fullscreen |
| F1 | Show the controls dialog |
| Esc | Close the backlog, cancel auto/skip, or exit |

## Save data

The save format depends on the game:

| Game | Format |
| --- | --- |
| D.C. II P.C., D.C.4, D.C.4 Plus Harmony | Native PC `.dat` files, with `.dat.arisa` companions for additional Arisa32 state |
| D.C. II Dearest Marriage | Native PC `.dat` + `.dat.arisa` files; existing portable saves remain loadable |
| D.C. II To You | Native PC `.dat` + `.dat.arisa` files with the compatibility limits below; existing portable saves remain loadable |
| Profiles without native PC save support | Portable `.dc4sav` files, with preview and metadata files |

Arisa32 can read and write the PC save format for the five titles above. The `.dat.arisa` companion stores additional state, including BGM and looping sound-effect playback positions. The position within a dialogue voice is not saved; voice replay starts from the beginning. PC games ignore this companion. Arisa32 can still load `.dat` files without it, but music may restart and some additional state will be unavailable.

For DC2TY, quicksave uses `dc2ty00.dat`, the latest manual-save copy uses `dc2ty01.dat`, and the 48 manual slots follow PC numbering: `02` through `04` on the first page, `08` through `12` on the second, then groups with an eight-number page stride. Save/Load rows show scenario titles and character icons. Tests confirm that the original readers and loader accept the save structure in isolated emulation; full scene restoration in the PC application has not been verified. Import rejects conditional overlays, background-plane chips, and nonzero background/pan offsets. See [DC2TY save limits and validation](docs/dc2ty-save-compatibility.md).

For DC2DM, quicksave uses `DC2DM01.dat`, autosave uses `DC2DM02.dat`, and the 100 regular slots use `DC2DM04.dat` through `DC2DM103.dat`. Arisa32 also reads the PC latest-save copy `DC2DM03.dat`, but does not refresh that separate copy; latest-save selection also considers regular slots. Save/Load rows show scenario titles, including titles from translated executables. Menu and gallery progress is imported from `system.dat`, then stored by Arisa32 in `system-profile.dat`. Exporting global progress back to `system.dat` is not available, and loading exported slots in the PC executable has not been directly verified. See the [DC2DM validation](docs/dc2dm-save-progress-validation.md).

On Windows, Arisa32 uses an existing game save directory, then tries to create one inside `AdvData`. It falls back to `build/desktop/saves/`. Set `DC4_SAVE_DIR` to choose another location. On Android, Arisa32 uses the game save directory when it is writable, or app-specific internal storage otherwise.

Android ZIP export offers a choice with or without `.dat.arisa`. Including companions provides a more complete backup for restoration in Arisa32.

## Development and diagnostics

These examples use D.C.4. Set the `AdvData` location through `DC4_ADVDATA` as described in the Windows instructions above.

```bat
rem Jump directly to a specific scene:
build\desktop\arisa32_desktop.exe --scene dc4_kyo20190204a2

rem Capture a 1280x720 screenshot without opening a window:
build\desktop\arisa32_desktop.exe --scene dc4_kyo20190204a2 --line 0 --shot work\scene.bmp

rem Inspect the title screen or test audio:
build\desktop\arisa32_desktop.exe --title
build\desktop\arisa32_desktop.exe --testaudio

rem Print all CLI options:
build\desktop\arisa32_desktop.exe --help
```

Run the CTest regression suites from an environment with a C++ compiler, CMake, and Ninja:

```sh
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop --parallel
ctest --test-dir build/desktop --output-on-failure
```

The DC2DM Android probe uses the APK's JNI library and copied PC saves in a separate temporary directory. It does not install the app or write to the game directory:

```sh
python tests/android/run_dc2dm_save_probe.py --apk path/to/app-debug.apk --saves path/to/copied/SaveData --serial DEVICE_SERIAL --data "/sdcard/Gamehub/Da Capo 2 Dearest Marriage/AdvData" --exe "/sdcard/Gamehub/Da Capo 2 Dearest Marriage/DC2DM.EXE" --font path/to/msgothic.ttc
```

## Repository layout

- `core/`: C++17 script VM, MES interpreter, CRX/CRM decoders, XPCM audio, and scene rendering.
- `sdl_desktop/`: SDL2 desktop application, window management, audio playback, dialogue backlog, and screenshots.
- `android/`: Android application, JNI bridge, `ArisaUi` components, and game library.
- `tools/`: The `mesdis` disassembler, `crx_tool`, `crm_tool`, and `voicetool` asset inspectors, and other analysis tools.
- `tests/`: Unit and regression tests for VM opcodes, save data, audio, and resource loading.
- `docs/`: Reverse-engineering documentation, opcode dispatch tables, and engine findings.

## Status and limitations

- The project covers CIRCUS adv32 scripts and assets for the Da Capo and Dal Segno series. Games using other engines are not supported.
- Dialogue flow, branching choices, character voices, background transitions, CGs, BGM, and system menus are implemented. Some opcodes and game behavior still need verification. The audit in [RE-HANDOFF.md](docs/RE-HANDOFF.md), section 12, recorded that about 39% of opcode instances lacked an explicit code path on October 13, 2026; that historical figure does not measure coverage in the current release.
- PC save compatibility is limited to the titles listed under [save data](#save-data).
- Arisa32 is an independent clean-room reverse-engineering project for research and preservation, not an official CIRCUS product. Development is ongoing.

## Copyright and trademarks

The names **D.C. ~Da Capo~**, **D.C. II**, **D.C. III**, **D.C.4**, **D.S. -Dal Segno-**, and **CIRCUS / adv32**, along with the related game assets, are CIRCUS intellectual property.

This project is intended for education, interoperability, and preservation. The repository does not include or distribute copyrighted game assets, including `.mes` scripts, `.crx`/`.crm` artwork, and `.xpcm`/`VOICE.DAT` audio. Users must provide their own legitimate copies of the games.

# Arisa32

[Bahasa Indonesia](README.md) | **English**

Arisa32 is a C++17 reimplementation of the CIRCUS adv32 visual novel runtime for the Da Capo and Dal Segno series. It runs on Windows with SDL2 and on Android with NativeActivity + JNI, reading MES scripts, CRX/CRM artwork, and XPCM audio directly from an installed game.

**Latest release: [v0.6.2](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.2)**. This release adds beta support for D.C. II Dearest Marriage, Sakuramusubi text fixes, and opening and ending playback fixes. Read the [release notes](docs/releases/v0.6.2/en.md) for details and remaining bugs. Version history is available in the [changelog](CHANGELOG.md).

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
- `D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~` (beta)

According to user testing, D.C. II Dearest Marriage is playable through to the end. Support remains in beta: menu fonts are too large, some popup text colors are incorrect, character voices may repeat, and other bugs remain. See the [known DC2DM bugs](docs/releases/v0.6.2/en.md#known-dc2dm-bugs).

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
- A bundled `MSGOTHIC.TTC` font for Japanese text, with options for system fonts and custom TTF/OTF/TTC fonts.
- Game removal from the library with an option to delete copied files, plus duplicate entry detection.
- An adv32/CIRCUS-themed `ArisaUi` interface with an Arisa crest icon.
- ZIP save archive import and export.
- Full-screen touch controls, dialogue backlog, and dialogue skipping.
- Android API 36 target, with support for Android 5.0 / API 21 and later, `arm64-v8a` and `armeabi-v7a`, and 16 KB memory pages.

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

Prebuilt APKs are also available on [GitHub Releases](https://github.com/SakuraSymphonyReTranslation/arisa32/releases). See the [Android guide](android/README.md) for permissions, storage setup, and installation through ADB.

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
| D.C. II Dearest Marriage | Portable `.dc4sav` files; PC save import is not supported yet |
| Profiles without native PC save support | Portable `.dc4sav` files, with preview and metadata files |

Arisa32 can read and write the PC save format for the three titles in the first row. The `.dat.arisa` companion stores additional state, including audio playback positions. PC games ignore this companion. Arisa32 can still load `.dat` files without it, but music may restart and some additional state will be unavailable.

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
- PC save compatibility is limited to the titles listed under [save data](#save-data). DC2DM still uses portable saves and has UI and voice bugs.
- Arisa32 is an independent clean-room reverse-engineering project for research and preservation, not an official CIRCUS product. Development is ongoing.

## Copyright and trademarks

The names **D.C. ~Da Capo~**, **D.C. II**, **D.C. III**, **D.C.4**, **D.S. -Dal Segno-**, and **CIRCUS / adv32**, along with the related game assets, are CIRCUS intellectual property.

This project is intended for education, interoperability, and preservation. The repository does not include or distribute copyrighted game assets, including `.mes` scripts, `.crx`/`.crm` artwork, and `.xpcm`/`VOICE.DAT` audio. Users must provide their own legitimate copies of the games.

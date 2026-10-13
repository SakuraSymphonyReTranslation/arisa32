# Arisa32

> [!IMPORTANT]
> ### 🤖 Full LLM Generated / 100% Dibuat oleh AI (LLM)
> Seluruh kode sumber, arsitektur sistem, reverse-engineering opcode MES adv32, dekoder aset, antarmuka Android/JNI, serta dokumentasi pada repositori ini **dibuat secara penuh oleh Large Language Model (LLM)**.
>
> ### 🎮 Kompatibilitas Game / Game Compatibility: Da Capo & Dal Segno Series (CIRCUS adv32)
> Runtime engine mendukung judul visual novel yang ditenagai oleh engine **CIRCUS adv32**, difokuskan secara khusus pada seri **Da Capo** dan **Dal Segno** (termasuk D.C.4, D.C.4 Plus Harmony, D.C. III series, D.C. II series, D.C. series, dan Dal Segno series). Judul non-adv32 (seperti D.C.4 Fortunate Departures yang menggunakan engine berbeda) tidak didukung.
>
> ### 📦 Nama Proyek / Repository: Arisa32
> Nama proyek dan repositori ini adalah **Arisa32** (pengganti penamaan internal sebelumnya `dc4engine`). Target paket Android menggunakan identifier `org.dc4engine` demi menjaga kompatibilitas save data dan path aplikasi.

A portable C++17 reimplementation of the CIRCUS adv32 visual novel engine runtime for **Da Capo & Dal Segno series** on Windows (SDL2) and Android (NativeActivity + JNI). It reads the original game's installed MES scripts, CRX/CRM artwork, and XPCM audio directly in place.

> [!NOTE]
> **Game assets are not included.** A legitimate copy of supported games is required. Game files (`AdvData`) are read in place and are never modified by the engine.

---

## Supported Games / Judul Game yang Didukung

Arisa32 mengenali dan mendukung 21 judul resmi dari seri Da Capo & Dal Segno berbasis engine CIRCUS adv32 dengan deteksi otomatis skrip dan aset:

### Da Capo 4 Series
- `D.C.4 ~Da Capo 4~`
- `D.C.4 ~Da Capo 4~ Plus Harmony` *(didukung sebagai judul terpisah dengan deteksi rute skrip khusus)*

### Da Capo III Series
- `D.C. III ~Da Capo III~`
- `D.C. III R ~Da Capo III R~`
- `D.C. III P.P. ~Da Capo III Platinum Partner~`
- `D.C. III With You ~Da Capo III With You~`
- `D.C. III Dream Days ~Da Capo III Dream Days~`

### Da Capo II Series
- `D.C. II ~Da Capo II~`
- `D.C. II Spring Celebration ~Da Capo II Spring Celebration~`
- `D.C. II P.C. ~Da Capo II Plus Communication~`
- `D.C. II To You ~Da Capo II To You~`
- `D.C. II Fall in Love ~Da Capo II Fall in Love~`
- `D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~`

### Da Capo (Original) Series
- `D.C. ~Da Capo~`
- `D.C.P.C. ~Da Capo Plus Communication~`
- `D.C. White Season ~Da Capo White Season~`
- `D.C. Summer Vacation ~Da Capo Summer Vacation~`
- `D.C.A.S. ~Da Capo After Seasons~`
- `D.C. Dream X'mas ~Da Capo Dream X'mas~`

### Dal Segno Series
- `D.S. -Dal Segno-`
- `D.S. i.F. -Dal Segno- in Future`

> [!NOTE]
> Seluruh variasi edisi (seperti Standard Edition, Limited Edition, Deluxe Edition, Download Edition) dikelompokkan ke dalam judul utama resminya. Judul non-adv32 seperti *D.C. 4 Fortunate Departures* tidak menggunakan engine adv32 sehingga tidak termasuk dalam lingkup runtime ini.

---

## Platforms

1. **Windows Desktop** (C++17 + SDL2)
   - Native 1280×720 letterboxed renderer with aspect ratio preservation.
   - Text rendering via Windows font rasterizer (MS Gothic by default or TrueType font override).
   - High-fidelity audio playback for BGM, SE, and voice streams (XPCM).
   - Original-style Save / Load system (100 slots + quicksave) and reading backlog.
   - Title screen, config menus, gallery, and scene selector.

2. **Android App** (NativeActivity + JNI + Java UI)
   - Built-in persistent Game Library manager with auto-detection for all 21 Da Capo & Dal Segno titles.
   - **External Storage Support**: Direct playback from MicroSD & USB OTG drives (`/storage/XXXX-XXXX`) without mandatory file copying, saving internal device storage.
   - **Pre-bundled Default Font**: Bundled `MSGOTHIC.TTC` font ensures crisp Japanese dialogue out of the box, with options for system default font or custom TTF/OTF/TTC fonts.
   - **Library Management**: Dedicated "REMOVE" button on game cards with prompt to optionally clean up copied files to reclaim space, plus duplicate detection.
   - Clean adv32 / Circus Engine UI theme (`ArisaUi`) with Arisa crest icon.
   - Save archive import/export system (`.zip` backup/restore).
   - Integrated full-screen touch controls, dialogue backlog, and fast-forward skipping.
   - Targets Android API 36 (compatible with Android 5.0 / API 21+; `arm64-v8a` & `armeabi-v7a`; 16 KB page-size ready).

---

## Build and Run on Windows

Install Visual Studio or Visual Studio Build Tools with **Desktop development with C++** and **C++ CMake tools for Windows**, then run:

```bat
build.bat
run.bat
```

The build helper detects Visual Studio and creates a Release x64 build in `build/desktop`. SDL2 headers and import libraries are included in `third_party/sdl2`; its runtime DLL is copied beside `build/desktop/dc4_desktop.exe` automatically.

The launcher automatically looks for `../DC4/AdvData` beside this repository. To use another installation directory, set `DC4_ADVDATA` or invoke the executable directly:

```bat
set "DC4_ADVDATA=D:\Games\DC4\AdvData"
run.bat
build\desktop\dc4_desktop.exe "D:\Games\DC4\AdvData"
```

The player starts the opening scenario, `dc4_kyo20190204a1.mes`.

---

## Build for Android

To build the standalone debug APK on Linux, macOS, or Windows (via Git Bash / MSYS2):

```bash
bash android/build_apk.sh          # -> generates android/Arisa32-debug.apk
```

Prerequisites:
- Android SDK build-tools 35.0.0 & platform `android-35`
- Android NDK 27.3+
- CMake 3.22.1+
- JDK 17+

Prebuilt binaries (`Arisa32-debug.apk`) are also provided on the [GitHub Releases](../../releases) page for manual sideloading. See [`android/README.md`](android/README.md) for full details on permissions, storage setup, and ADB installation.

---

## Reading Controls (Desktop)

| Input | Action |
| --- | --- |
| Left click, Space, Enter | Finish the typewriter text, then advance |
| Right click, H, OFF/Close button | Hide/show the message window |
| A, AUTO button | Auto advance after text and voice finish |
| S, SKIP button; hold Ctrl | Skip dialogue |
| V, VOICE button | Replay the current voice line |
| F5, QUICK SAVE button | Write quick-save snapshot |
| F9, QUICK LOAD button | Restore quick-save snapshot |
| SAVE / LOAD buttons | Open 100-slot Save/Load screen |
| 1–9 / 0 in Save/Load | Select slots 1–10 on current page |
| Left/Right, Page Up/Down in Save/Load | Change page (10 pages, wrapping) |
| Tab in Save/Load | Switch between Save and Load screens |
| B, BACKLOG button, Mouse wheel up | Open reading backlog history |
| Mouse wheel, Arrow keys, Page Up/Down | Scroll backlog history |
| Click a choice, or press 1–9 | Select an option |
| F11, Alt+Enter | Toggle fullscreen |
| F1 | Show controls dialog |
| Esc | Close backlog, cancel auto/skip, or exit |

---

## Save Data & Snapshots

- Snapshots use Arisa32's internal portable format (`.sav` snapshot files with metadata sidecars).
- Quick saves and the 100 manual slots live in `build/desktop/saves/` on Windows, or in app-isolated external files storage on Android.
- **Original Windows D.C.4 save files (`AdvData/savedata`) are not compatible.**
- The original game's save directories are never altered.

---

## Development & Diagnostics

```bat
rem Jump directly to a specific scenario:
run.bat scene dc4_kyo20190204a2

rem Headless reproducible screenshot at native 1280x720:
build\desktop\dc4_desktop.exe --scene dc4_kyo20190204a2 --line 0 --shot work\scene.bmp

rem Inspect title screen or test audio:
run.bat title
run.bat audio

rem Print all CLI options:
build\desktop\dc4_desktop.exe --help
```

To run the automated CTest regression test suites:
```sh
cmake -S . -B build/desktop -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop --parallel
ctest --test-dir build/desktop --output-on-failure
```

---

## Repository Layout

- `core/`: Portable C++17 script VM, MES interpreter, CRX/CRM decoders, XPCM audio, and scene renderer.
- `sdl_desktop/`: SDL2 desktop frontend, window management, audio playback, backlog, and screenshot capture.
- `android/`: Android frontend (NativeActivity, JNI glue, `ArisaUi` custom widget library, Game Library activity).
- `tools/`: Disassemblers (`mesdis`), asset inspection utilities (`crx_tool`, `crm_tool`, `voicetool`), and analysis helpers.
- `tests/`: Automated unit and regression tests for VM opcodes, save states, audio, and resource loading.
- `docs/`: Reverse-engineering documentation, opcode dispatch tables, and engine findings.

---

## Current Status & Limitations

- **CIRCUS adv32 Scope**: Tested against CIRCUS adv32 visual novel assets and scripts (Da Capo & Dal Segno series). Non-adv32 games (such as *D.C. 4 Fortunate Departures*) use a completely different engine architecture and are not supported.
- **Opcode Coverage**: Core gameplay, dialogue flow, branching choices, voice playback, background transitions, CG displays, BGM, and system menus are functional. Roughly 39% of less common opcodes fall through to default stub behavior (see `docs/RE-HANDOFF.md` §12).
- **Save Compatibility**: Saves cannot be imported from or exported to the original Windows binary.
- **Work in Progress**: Not an official product of CIRCUS. This is an independent, clean-room reverse-engineering project created entirely with LLMs for preservation and research purposes.

---

## Legal & Trademark Notice

- **D.C. \~Da Capo\~**, **D.C. II**, **D.C. III**, **D.C.4**, **D.S. -Dal Segno-**, and **CIRCUS / adv32** are trademarks and copyrighted intellectual property of **CIRCUS**.
- This project is an independent, clean-room reimplementation developed strictly for educational, interoperability, and preservation purposes.
- No copyrighted game assets, scripts (`.mes`), images (`.crx`/`.crm`), or audio streams (`.xpcm`/`VOICE.DAT`) are bundled or distributed with this repository. Users must own and provide their own legitimate copy of the games.


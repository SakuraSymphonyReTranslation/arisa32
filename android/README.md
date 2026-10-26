# Arisa32 Android app (dc4engine)

The app opens to a persistent game library. Selecting **Play** opens the game's
original title screen, rendered by the same portable `dc4core` used on desktop.
Game assets and translation packs are supplied separately from the APK.

The launcher icon and library header use the supplied character artwork, with
legacy density variants and an adaptive icon for current Android launchers.
The header keeps a fixed 40 dp square aligned vertically with the Arisa32 title.

## Build

```bash
bash android/build_apk.sh          # -> android/Arisa32-debug.apk
```

Requires Android SDK build-tools 35.0.0, platform android-35, CMake 3.22.1,
NDK 27.3.13750724 and JDK 17+. Override defaults with `ANDROID_SDK`,
`ANDROID_NDK_HOME`, `JAVA_HOME` and `DC4_BUILD_DIR`. The packaging script clears
`DC4_BUILD_DIR`; use a dedicated build directory, never a source or game-data
folder. Gradle/Android Studio can also build `android/`.

The APK targets API 36 (Android 16), supports Android 5.0/API 21 onward, and contains
`arm64-v8a` and `armeabi-v7a` libraries. The standalone script compiles every
Java helper with UTF-8 source encoding. C++ exceptions must remain enabled for
checked scene snapshots.

## Add and choose games

- **Add game folder → Choose folder** uses Android's Storage Access Framework.
  Choose `AdvData`, its parent, or a translation root containing `MES`. If the
  directory is accessible via direct filesystem paths, the app offers instant
  **Direct Playback** without copying, preserving internal flash storage.
  Alternatively, choosing **Copy to App Storage** copies files into app-owned
  storage on a worker thread with progress and cancellation.
- **External Storage (MicroSD / USB OTG)** provides automatic scanning and a
  built-in directory browser for external drives (`/storage/XXXX-XXXX`), running
  directly in place with zero disk copying.
- **Auto-Detection**: The library recognizes 21 official Da Capo & Dal Segno
  titles by inspecting MES scripts and directory fingerprints (e.g. Asumi/Alice
  route detection differentiates D.C.4 Plus Harmony from standard D.C.4).
- **Library Management & Duplicate Prevention**: Game cards feature dedicated
  **LAUNCH**, **MANAGE**, and **REMOVE** actions. Removing an entry gives the
  option to delete imported files from internal storage to reclaim space, or
  safely remove from the list while keeping original external files untouched.
  Registering paths automatically checks for existing entries to prevent duplicates.
- The complete game needs `MES/start.mes` (or valid start scenario for the title),
  `GRP/SYSTEM/MESWIN.CRX`, system-menu scripts/artwork, and the remaining `GRP`,
  `MUSIC`, `MOVIE`, `NAMES.DAT` and `VOICE.DAT` resources. Missing required
  resources produce a diagnostic.
- On an empty library, readable storage paths or a valid `data_dir` intent extra
  are registered automatically. Startup still shows the library.

The catalogue is atomically persisted in `files/library.properties`. Game
identity is SHA-256 over sorted relative MES script names and contents, so moving
unchanged data does not detach its saves. Interrupted catalogue writes recover
from their backup.

```bash
adb install -r android/Arisa32-debug.apk
adb shell am start -n org.dc4engine/.Arisa32Activity \
  --es data_dir /storage/emulated/0/DC4PH/AdvData
adb logcat -s Arisa32 dc4
```

## Languages and fonts

**App language** selects English, Indonesian or Japanese for the library,
Android settings and confirmation dialogs. This is independent of **Game
language**, which selects original Japanese or an imported English/Indonesian
translation pack. The original title/config/gallery/music artwork keeps its
language unless the selected pack supplies translated assets.

Use **Game settings → Import language pack**, select its language and text
encoding (Windows-31J/CP932 or UTF-8), then choose its folder. The pack must
contain actual MES scripts; empty placeholder directories are rejected. Select
it under **Game language** after import. `GRP`, `MES` and `MOVIE` resolve from
the overlay first, with case-insensitive matching and per-file base fallback.
Audio, saves and unrelated paths are not redirected. UTF-8 uses strict decoding
with CP932 fallback for untranslated strings.

Each game remembers its content language and dialogue font.
- **Default Font (`MSGOTHIC.TTC`)**: Bundled directly within the APK assets and
  automatically extracted to ensure standard Circus visual novel typography
  out of the box.
- **System Default & Custom Fonts**: Users can toggle between MS Gothic, the
  Android system default font, or import custom `.ttf`, `.otf`, or `.ttc` fonts
  through Android's document picker.
Fonts are size-limited, checked for a supported font signature and loaded by
Android before installation. A changed font applies on the next game launch.
Changing content language returns to the library; save progress first. Separate
language save directories prevent accidentally mixing translated script offsets.

## Original screens and engine controls

The native MES-driven title, extras, seven-page config, CG gallery, music room,
scene replay, backlog and numbered save/load screens share the original game
assets and portable controllers. Menu availability follows recorded progress.
The native config controls dialogue/auto timing, read/unread skipping, stopping
at choices, voice waiting, window opacity, audio groups/character volumes and
supported effects. Android retains its landscape/fullscreen presentation for
platform-specific window controls.

A draggable **⋮** bubble opens engine-only extras: original config, language and
font settings, save transfer, original title, or library. It stays inside the
upper part of the window, away from the message-bar buttons. Its position is
remembered. Android Back opens the same extras while playing. Returning to the
library joins rendering, stops audio and destroys the current engine before
another game can start.

Scene touches use the same centered 16:9 viewport as rendering. Press/release
must hit the same original control or choice; cancelled gestures and secondary
fingers cannot activate them. Dialogue taps reveal or advance. Auto/skip follow
native config and pause at a choice until the player selects it. Backlog rows
support scrolling, voice replay without closing the log, and snapshot-based
scene jumps. Slot menus have 100 numbered slots with timestamps, 128×72 previews
and scenario titles; overwrites require a confirmation and queue the write
onto the render thread.

MPEG-1 Program Stream movies and MPEG Layer II sound decode through portable
`pl_mpeg` and the core mixer, including scripted movie completion and tap-to-skip.
COMU, camera animation, transitions and credits use the shared scene renderer.
Other video codecs are outside this decoder's scope.

## Saves and portable archives

Emulator data lives under:

```
files/games/<base-script-sha256>/saves/<jp|en|id>/
```

Generic profiles contain `slot1.dc4sav` through `slot100.dc4sav`, `quicksave.dc4sav`, an
`autosave.dc4sav` if present, preview sidecars, `system-profile.dat` for native
settings/unlocks and `read-lines.txt` for read history. Existing legacy
`files/saves` slots are copied once to the first registered game's Japanese
save directory; the original legacy copies remain intact.

DC2PC reuses the game's `SaveData` folder; DC4 and Plus Harmony use the selected
language save directory. These profiles write native PC `.dat` records with
100 emulator slots. Quick/first regular filenames are `DC2PC00.dat`/`DC2PC02.dat`,
`DC402.dat`/`DC403.dat` and `DC4PHDL02.dat`/`DC4PHDL03.dat`. Native files take
priority over old emulator slots. Checksum-bound `.dat.arisa` companions retain
additional emulator state and are ignored after the PC changes the `.dat`.
DC2PC keeps `DC2PC01.dat` for the PC's latest-save copy. Regular slots map to
physical 02..04, 08..12, 16..20 and subsequent five-record groups with an
eight-record page stride. The original ten-page menu exposes 48 regular slots;
the additional emulator slots have filenames beyond that menu's range.
Previews use native stems (for example `DC403.png`). Scenario-title descriptions
are resolved from the scenario identity in both old snapshots and native saves.

**Game settings → Export saves / Import saves** uses Android document providers
and a versioned ZIP archive. Exports include manual/quick/automatic saves,
previews, native progress/profile and read history. Import closes the running
game and replaces manual/quick slots for the selected content language. Included
profile/read-history data is restored on next launch. Older slot-only archives
preserve the existing profile, read history and automatic save. The selected
archive includes native `.dat` records and `system.dat` where present. Export
opens a dialog with two options: **With .dat.arisa** (the default full backup,
retaining additional Arisa32 state such as audio playback positions) and
**Without .dat.arisa** (a smaller archive that omits only these companions).
Both keep other save records, previews, settings, unlocks and read history.
Supported native `.dat` saves still load in Arisa32 without companions, but
music may restart and additional emulator state is not retained. Original PC
games ignore companions. The export popup shows the selected mode, current
filename and file-count progress, followed by a completion dialog. Suggested
ZIP filenames distinguish `with_arisa` and `without_arisa` exports.
The archive importer currently skips manifest identity validation; choose the
matching game and language. Native exports support dialogue/choice checkpoints,
local variables, backgrounds, supported sprites and music/looped effects.
Some animated graphics state remains unsupported, and actual loading by each PC
executable must be verified before claiming full compatibility.

Import rejects traversal/absolute paths, unknown or duplicate entries, oversized
content, invalid scene-save versions/lengths/checksums, orphan previews and
malformed profile/read-history records. Limits include 310 entries, 32 MiB per
file and 256 MiB total extracted archive data, with tighter native save and
preview limits. All validation occurs in a sibling staging directory before the
existing save directory is renamed to a backup. Failed swaps roll back; startup
recovers interrupted swaps. Export a backup before replacing a save set.

## Lifecycle and validation

Live input, rendering, original-menu changes and save actions run on one render
thread. Lifecycle transitions stop/join it before native pause or shutdown.
Backgrounding pauses the audio device and movie decoder while retaining scene,
VM, mixer and decoder state in memory; returning resumes that state. The default
Keep session setting also stores a per-game/language checkpoint for process
restart. External movie playback survives app switching while the process stays
alive; movie restoration after process termination is currently unsupported.
File import/export runs on a separate worker while native rendering is stopped.

The Java shell compiles against Android API 36. The profile harness in
`tests/android/ProfileChecks.java` verifies native filenames, fallback priority,
save discovery, migration and scenario-title extraction. The older archive
harness still expects strict manifest validation and currently fails that check.
Portable resource and in-memory snapshot tests
live in `tests/resource_test.cpp` and `tests/scene_test.cpp`. Android hardware QA,
APK signing/alignment and full route coverage must be reported separately from
these compile/helper tests; passing them alone does not establish complete
playability or device-specific storage, font and lifecycle behavior.

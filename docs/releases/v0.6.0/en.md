[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.0 - D.C. II P.C. Support and Native Saves

Released 2026-10-26.

### D.C. II P.C. (Plus Communication) support

- **D.C. II P.C. \~Da Capo II Plus Communication\~ is officially supported.** Support covers scripts and text, dialogue/choices, character images and expressions, music/voices, the title screen, sidebar, Save/Load, backlog, config, and Extra/replay menus.
- Fixes to Extra menu navigation, replay scrolling, restoration of older snapshots, and Android Back/sidebar controls.
- Saved progress and route flags are restored without turning zero-valued flags into unlocks.

### Saves and restoration

- DC2PC, D.C.4, and D.C.4 Plus Harmony store slots in PC `.dat` format, with a `.dat.arisa` companion for additional Arisa32 state.
- Save numbering follows each game's format. DC2PC uses native page numbering and keeps `DC2PC01.dat` as the Latest copy, separate from regular slots.
- Existing SaveData folders are reused. Older `.dc4sav` saves remain a fallback and are migrated without overwriting existing slots.
- Slots display scenario titles and thumbnails. A bundled title table speeds up the first opening of the Load menu.
- Fixes to character position, scale, pivot, cropping, and camera state when loading native D.C.4/Plus Harmony saves.
- Fixes to Save/Load announcement voices that could start twice, and to transition timing when the app is paused/resumed.

### Save export

- The export popup offers **With .dat.arisa** for a full Arisa32 backup and **Without .dat.arisa** for a smaller archive.
- Supported games can still load `.dat` saves without the companion file. Audio position and some additional Arisa32 state are not restored, so music may restart. PC games ignore `.dat.arisa` files.
- Both options include other saves, previews, settings, unlocks, and read history when available.
- The progress popup shows the export mode, file name, percentage/file count, and completion confirmation. ZIP names distinguish `with_arisa` and `without_arisa` modes.

### App appearance

- The default app icon uses new character artwork, with adaptive icons and sizes for different Android screen densities.
- The icon beside the Arisa32 title in Game Library uses the same artwork, sized and aligned with the title.

### Validation and notes

- **15/15 C++ core tests, 288 Android profile checks, and 217 ZIP export checks passed.** Debug and release APKs built successfully. Sample native DC2PC/D.C.4/Plus Harmony saves were also loaded and continued during device/PC testing.
- Some animation/renderer state and native history still have limitations; sample results do not guarantee every checkpoint. Restoring a movie after the app process ends is not yet supported.
- The APK supports Android 5.0+ (`arm64-v8a` and `armeabi-v7a`). Game files and translation packages are supplied separately.

### Downloads

[Open the v0.6.0 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.0)

- `Arisa32-v0.6.0.apk`: release installation package for regular use.
- `Arisa32-v0.6.0-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

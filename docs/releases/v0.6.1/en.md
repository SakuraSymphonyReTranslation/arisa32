[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.1 - Automatic Updates, Dynamic FPS Meter, and Replay Save/Load Controls

### New features and UI improvements

- **Automatic update checks**:
  - Arisa32 checks the GitHub repository for a newer release when the app starts, if the option is enabled.
  - A new release is announced through an Android system notification, a toast, and an interactive dialog with release notes and a button to download the latest release APK.
  - Settings includes an On / Off option and a button to check for updates manually.
- **Dynamic FPS meter at the game frame corner**:
  - A real-time frame-rate counter updates every 500 ms using render deltas for stable readings.
  - The meter follows the bounds of the active visual novel frame:
    - For 4:3 games such as D.C. II P.C. (`dc2pc`), it sits at the upper-right corner of the game area (`X = 1120`), outside the black pillar bars.
    - For 16:9 games such as D.C.4 and D.C.4 Plus Harmony (`dc4` / `dc4ph`), it sits at the upper-right corner of the full screen (`X = 1280`).
    - It adapts to movie cutscenes with different aspect ratios and to windowed/fullscreen changes.
  - An On / Off switch is available in the app Settings and the in-game bubble menu (Extra menu).

### FFmpeg video backend and APK size

- **Video playback migrated from LibVLC to FFmpeg**:
  - Replaces LibVLC with a custom FFmpeg build containing the components, demuxers, and video/audio codecs needed by the visual novel engine: MPEG-1/2, H.264, WMV, AAC, MP3, Vorbis, and WMA.
  - Reduces APK size by removing unused external modules.
  - Integrates rendering into the native Arisa32 graphics pipeline with audio/video frame synchronization and more efficient memory use.

### Save/Load restrictions in Scene Replay and Scenario Mode

- **Save, Load, QuickSave, and QuickLoad are disabled during Replay/Scenario Mode**:
  - Following the native Windows Circus engine behavior, these operations are disabled while Scene Replay or Scenario Replay is active.
  - QuickSave/QuickLoad and related taskbar/sidebar actions are hidden or disabled to prevent changes to save data or story flags while replaying scenes.
  - `save_state` and `load_state` operations are rejected when the replay flag is active.

### Engine stability

- Rendering maintenance for the camera, Event CGs, backgrounds, and character placement across supported game profiles.
- All 18 desktop unit-test suites (`ctest`) passed.

### Downloads and checksums

[Open the v0.6.1 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.1)

- `Arisa32-v0.6.1.apk`: release installation package for regular use.
- `Arisa32-v0.6.1-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

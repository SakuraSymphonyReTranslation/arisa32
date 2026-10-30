[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.5.0 - CIRCUS adv32 Runtime for the Da Capo and Dal Segno Series

> [!IMPORTANT]
> ### Generated entirely by AI (LLM)
> All source code, system architecture, adv32 MES opcode reverse engineering, asset decoders, Android/JNI interfaces, and repository documentation were created by a Large Language Model (LLM).
>
> ### Game compatibility: Da Capo and Dal Segno (CIRCUS adv32)
> This runtime supports the CIRCUS adv32 visual novel engine, with a focus on the Da Capo and Dal Segno series.

## Release notes: v0.5.0

### LibVLC video playback

- Integrates the official VideoLAN library `org.videolan.android:libvlc-all:3.6.5`, including its internal FFmpeg software decoder, for MPEG-2 (`mpeg2video`) and MP4 support.
- Fixes the black screen in D.C.4 Plus Harmony Opening 3. The `DC4_OP3.mpg` file from the VNDB-linked fan translation patch, which contains MPEG-2 Main Profile video in an MP4 container, can play with synchronized video and audio without offline re-encoding.
- Uses hardware-backed `SurfaceView` rendering through `VLCVideoLayout` and Android's SurfaceFlinger compositor for video playback.
- Plain MPEG-1 Program Stream video continues to use C++ `pl_mpeg`. MP4/MKV containers and codecs such as MPEG-2, MPEG-4, H.264, AAC, and AC3 use LibVLC automatically, with MediaPlayer as a fallback.

### Patch video directory priority and fallback

- Video search follows the selected patch language:
  - Indonesian (`ID PATCH`): prioritizes `id_Data/Movie` / `id_Data/MOVIE`.
  - English (`ENG PATCH`): prioritizes `eng_Data/Movie` / `eng_Data/MOVIE`.
  - Original Japanese (`JP Original`): prioritizes `AdvData/Movie` / `Advdata/MOVIE`.
- If a video is missing from the selected patch directory, the search falls back to the main `AdvData/Movie` or `Advdata/MOVIE` directory.
- `.mpg` and `.mpeg` files take priority over other video formats.

### Game profiles and directory scanning

- Replaces fixed scanning logic with `GameProfileRegistry`, `Dc4Profile`, `Dc4PlusHarmonyProfile`, and `GenericCircusProfile`.
- Supports recursive scanning across internal storage, external SD cards, USB OTG, `Gamehub`, `Games`, `Download`, and user-selected folders.

### SaveData and `system-profile.dat` creation

- If `SAVEDATA` / `savedata` / `SaveData` is missing or has been deleted, Arisa32 detects the game profile and recreates the folder with `mkdirs()`.
- During game initialization (`dc4Init`), the engine immediately creates a default `system-profile.dat` when it is missing, preparing configuration and system flags.
- This behavior applies to CIRCUS adv32 titles, including D.C.4, D.C.4 Plus Harmony, D.C.3, D.C.2, and Dal Segno.

### Build and system compatibility

- Uses compile SDK 36, `versionCode 5`, and `versionName '0.5.0'`.
- Adds the Gradle packaging rule `pickFirsts += ['**/libc++_shared.so']` to resolve the shared C++ STL dependency between CMake and LibVLC.

### Downloads

[Open the v0.5.0 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.5.0)

- `Arisa32-v0.5.0.apk`: standalone Android release package, supporting Android 5.0+ / API 21-36 on `arm64-v8a` and `armeabi-v7a`.
- `Arisa32-v0.5.0-debug.apk`: standalone debug package with verbose logcat logging enabled.

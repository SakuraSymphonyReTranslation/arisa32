[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.4.0 - CIRCUS adv32 Runtime for the Da Capo and Dal Segno Series

> [!IMPORTANT]
> ### Generated entirely by AI (LLM)
> All source code, system architecture, adv32 MES opcode reverse engineering, asset decoders, Android/JNI interfaces, and repository documentation were created by a Large Language Model (LLM).
>
> ### Game compatibility: Da Capo and Dal Segno (CIRCUS adv32)
> This runtime supports the CIRCUS adv32 visual novel engine, with a focus on the Da Capo and Dal Segno series.

## Release notes: v0.4.0

### Opening video playback and black screens

- Identified that D.C.4 Plus Harmony's `DC4_OP3.mpg` uses `mpeg2video` (XDCAM EX), which many Android hardware decoders cannot display, resulting in audio without video.
- Splits movie selection into `resolve_movie_file`, `is_op3_movie`, `is_opening_movie`, and `get_base_movie_dir`. Opening 3 prioritizes H.264 MP4 (`.mp4`), while other opening videos prioritize `.mpg`.
- Opening videos load directly from the original game's `Advdata/MOVIE` directory to avoid conflicts with translation overlay files.

### Save handling and location

- Prioritizes save storage inside `AdvData` (`Advdata/SAVEDATA` or `AdvData/savedata`), following the original Circus engine layout.
- `extractSlotNumber` prevents slot-number collisions: `DC401.dat` no longer matches `DC411.dat`, and `DC404.dat` no longer matches `DC414.dat`.
- Removes automatic file-deletion routines from save handling to preserve user save files.

### Load screen image previews

- Native NDK `copy_thumbnail_bitmap` supports both `RGBA_8888` and `RGB_565`, with automatic dimension and stride adjustment.
- `findThumbnailImage` searches `.png`, `.PNG`, `.jpg`, `.JPG`, `.jpeg`, and `.JPEG` files.
- Save/Load slots display full-color screenshot thumbnails.

### Android compatibility and media access

- Uses compile SDK 36 and declares `android:enableOnBackInvokedCallback="true"` for predictive back gesture animations.
- Adds granular media permissions (`READ_MEDIA_AUDIO`, `READ_MEDIA_VIDEO`, `READ_MEDIA_IMAGES`) for Android 13 and later.

### Opcode dispatch structure

Splits `SceneRunner::advance_to_next_text()` into dedicated dispatchers:

- `dispatch_timing_command()`: timers, waits, and synchronized transitions.
- `dispatch_flow_command()`: script navigation, jumps, calls, and branches.
- `dispatch_layer_command()`: background layers, surfaces, and blend state.
- `dispatch_sprite_command()`: standing character slots, coordinates, and expressions.
- `dispatch_camera_command()`: camera movement, panning, and zoom.
- `dispatch_ui_chip_command()`: interactive UI elements, button chips, and quick-menu widgets.
- `dispatch_text_command()`: dialogue rendering, character names, and voice tags.

### Sprite stacking and lingering characters

- Corrects opcode `0x40` using reverse-engineering findings from `DC4.EXE` (VA `0x42CD43`). It targets cut-in slot 25 (`effect_sprite_`), rather than sprite slot 0 (`sprites_[0]`). This fixes characters from previous scenes, such as Suginami, reappearing when a cut-in effect is called.
- On scene transitions (`0x37`, `0xB4`, `0x49`, `0x30`), inactive sprite slots are automatically cleared and hidden.

### Downloads

[Open the v0.4.0 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.4.0)

- `Arisa32-v0.4.0.apk`: standalone Android release package, supporting Android 5.0+ / API 21-36 on `arm64-v8a` and `armeabi-v7a`.
- `Arisa32-v0.4.0-debug.apk`: standalone debug package with verbose logcat logging enabled.

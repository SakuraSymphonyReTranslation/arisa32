[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.3.0 - CIRCUS adv32 Runtime for the Da Capo and Dal Segno Series

> [!IMPORTANT]
> ### Generated entirely by AI (LLM)
> All source code, system architecture, adv32 MES opcode reverse engineering, asset decoders, Android/JNI interfaces, and repository documentation were created by a Large Language Model (LLM).
>
> ### Game compatibility: Da Capo and Dal Segno (CIRCUS adv32)
> This runtime supports the CIRCUS adv32 visual novel engine, with a focus on the Da Capo and Dal Segno series.

## Release notes: v0.3.0

### In-scene video and cutscenes in D.C.4 / D.C.4 Plus Harmony

- Opening videos, introduction animations, and in-game cutscenes now play automatically from adv32 scripts.
- Video decoding and frame presentation integrate with `SceneRunner` and Android Surface rendering.

### Notification popup text colors

- Fixes text-color rendering in in-game notification popups, including quick-save messages, the auto-mode indicator, and system banners.
- Aligns ARGB/RGBA channel handling with adv32 shader blending.

### D.C.4 Plus Harmony compatibility

- Addresses data-reading issues in **D.C.4 ~Da Capo 4~ Plus Harmony**.
- Supports archives larger than 2 GB through 64-bit seeks for `VOICE.DAT`, and registers the `LVOICE\\` audio partition prefix.
- Detects Plus Harmony's exclusive route scripts (`dc4_asu*`).

### BGM switching

- Background music changes automatically when moving between the main menu, title screen, and scenario. Tested with D.C.4 and D.C.4 Plus Harmony.

### Removing games from the library

- Adds a **REMOVE** action to Game Library cards.
- Users can remove registered games. For games copied to internal storage, an option also removes the copied game data to free storage space.

### Direct external storage access

- Runs games directly from MicroSD cards and USB OTG drives without copying game files into internal storage.
- Supports automatic volume scanning and external directory navigation.

### Character introductions and PCK transitions

- Transition animations such as character introductions (`AdvData\GRP\EFF\*.PCK`) follow their original duration.
- A touch guard prevents accidental skipping when the screen is touched during dialogue/transitions.

### Experimental support and other series

- **D.C. II ~Da Capo II~ Dearest Marriage:** Support remains experimental, with work on button-chip parsing and opening menu navigation.
- **Other Da Capo and Dal Segno titles:** The catalog recognizes these games, but full script and asset compatibility is still being tested and has not been comprehensively verified.

### Downloads

[Open the v0.3.0 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.3.0)

- `Arisa32-v0.3.0.apk`: standalone Android release package, supporting Android 5.0+ / API 21-36 on `arm64-v8a` and `armeabi-v7a`.
- `Arisa32-v0.3.0-debug.apk`: standalone debug package.

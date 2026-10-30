[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.5.5 - CIRCUS adv32 Runtime for the Da Capo and Dal Segno Series

> [!IMPORTANT]
> ### Generated entirely by AI (LLM)
> All source code, system architecture, adv32 MES opcode reverse engineering, asset decoders, Android/JNI interfaces, and repository documentation were created by a Large Language Model (LLM).
>
> ### Game compatibility: Da Capo and Dal Segno (CIRCUS adv32)
> This runtime supports the CIRCUS adv32 visual novel engine, with a focus on the Da Capo and Dal Segno series.

## Release notes: v0.5.5

This hotfix addresses audio/video synchronization during Kotori's song performance in the prologue of **D.C.4 ~Da Capo 4~** and **D.C.4 ~Da Capo 4~ Plus Harmony**.

### Kotori prologue audio fixes

#### Issues addressed

- **Interrupted audio and repeated lyrics:** The prologue could play the opening lyrics twice ("kimi no tsusumu kimi no tsusumu chisana sakura no yume"). The script's intro wait was too long, allowing BGM 43 (`043.pcm`) to reach the vocals before the video started. BGM then stopped abruptly as `KotoRI_uta01.mpg` began playing its own audio.
- **Silence after the video:** When the 22.7-second animation ended, the song stopped, leaving the following dialogue silent. In the PC version, Kotori's song continues through the dialogue until the `BG\BLACK` transition.

#### Implementation

- **Opcode `0x15(72)` timing:** Sets the intro wait to **12,580 ms (12.58 seconds)** for D.C.4 and D.C.4 Plus Harmony, aligning with the start of BGM 43's vocals at 12.70 seconds and the performance video's beat.
- **One continuous audio source:** During `KotoRI_uta01.mpg` / `KotoRI_uta02.mpg`, the engine keeps BGM 43 running by skipping `stop_group(Bgm)`. The video is muted and uses visual clock synchronization. BGM 43 continues through the singing dialogue until the scenario transition finishes.
- **Touch-skip synchronization:** Tapping to skip the video advances BGM 43 with `seek_pcm` to the position corresponding to the video's end, keeping the following dialogue synchronized.
- **Java player handling (`Arisa32Activity.java`):** Adds `isKotoriSongMovie` checks to the MediaPlayer and LibVLC fallbacks to mute video audio (`setVolume(0)`) and avoid taking audio focus from native BGM 43.
- **Game-specific guard:** The fix uses `is_dc4_or_dc4ph_game(...)` and script/file signatures (`dc4_start.mes`, `dc4_sen20181213b.mes`, `DC4.exe`, `DC4PHDL.exe`) to apply this behavior only to D.C.4 and D.C.4 Plus Harmony, keeping other Circus titles outside this code path.

### Downloads

[Open the v0.5.5 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.5.5)

- `Arisa32-v0.5.5.apk`: standalone Android release package, supporting Android 5.0+ / API 21-36 on `arm64-v8a` and `armeabi-v7a`.
- `Arisa32-v0.5.5-debug.apk`: standalone debug package with verbose logcat logging enabled.

[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.7 - COMU, FXAA on Android 5+, and Display Settings

Version 0.6.7 fixes phonechat/COMU in D.C.4 and D.C.4 Plus Harmony, makes FXAA available on Android 5 and later, and adds font size, visual effect, and screen mode settings. This release also fixes voice replay references and character drawing order.

### Phonechat / COMU

- Preserves manual spaces and line breaks in patches that position their own text, including the Indonesian patch.
- Recognizes Sakuramusubi's word separators and 11 romanized sender names. Messages wrap at word boundaries and use the corresponding sender portraits.
- Proportional fonts use their natural letter spacing and sizes up to 22 pixels, with up to three lines. With the tested Pretendard JP Bold `default.ttf`, the 505 messages use sizes from 18 to 22 pixels to make lowercase letters easier to read.
- MS Gothic retains its fixed-grid COMU layout with up to two lines. COMU sizes text to its chat bubbles independently of the regular dialogue size setting.

### Display and fonts

- FXAA is available on Android 5 through 12 using OpenGL ES 2.0. Android 13 and later retain RuntimeShader rendering. Off and Bilinear remain available; Bilinear is the default and the fallback when FXAA cannot be used.
- Choose Fit to Screen to preserve proportions or Stretch to fill the display area. The setting persists per game, applies immediately during play, and also controls touch mapping, overlays, and video presentation. Black margins authored into the game frame remain visible.
- DC4/DC4PH dialogue sizes from 18 to 22 are available for MS Gothic, system fonts, and custom fonts. The value persists per game and applies when the game is reopened; the default remains 22.

### Visual effects, voices, and characters

- Restores falling sakura and floating lights from the game's script commands and original atlases, including animation, density, and effect replacement. PC save import/export and new Arisa32 saves retain supported effect state.
- The Visual effect setting controls petals and floating lights in DC4/DC4PH. Changes apply immediately during play; turning effects off hides and pauses their particles. Sakura Off preferences from earlier test builds are preserved.
- Narration and unvoiced dialogue no longer inherit the previous character's voice replay reference. Ongoing audio still follows the playback settings.
- Sprite order follows script-defined priority, including Gen appearing behind Mitsumi. Priority survives hiding, replacement, and save restoration.

### Progression, validation, and limitations

Progression checks extend coverage of the fixes from 0.6.5. The Sorane Ending 2 choice in DC4/DC4PH follows the Arisu Ending 1 requirement; completed PC data still unlocks it. The D.C. menu follows its eight required ending flags. This release does not automatically erase users' completion progress.

Testing covers the COMU renderer with 505 messages, desktop/Android image comparisons, voices, sprites, effects, saves, progression, screen settings, and font choices. The GLES path for older Android versions ran on an Android 16 device and was checked against minimum API 21. Direct testing on Android 5 through 12 devices has not been completed; compatibility with every older driver, FPS, and battery use remain unverified.

These checks are not full playthroughs of every route or patch. Old saves containing incorrect COMU senders or voice references are not automatically migrated; load a point before the affected conversation to record new state. DC2 Dearest Marriage support remains beta. Game files, translation patches, and custom fonts are provided separately.

See the [0.6.7 release verification](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.7/docs/release-qa-0.6.7.md) for results and test limits.

### Downloads and installation

[Open the v0.6.7 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.7)

- `Arisa32-v0.6.7.apk`: release package for regular use.
- `Arisa32-v0.6.7-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Install the update without uninstalling the app to retain data. In-app updates are available since 0.6.4; Android still requests installation confirmation. Android 5.0+; arm64-v8a and armeabi-v7a.

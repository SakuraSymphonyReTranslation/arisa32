[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.7.5 - Dal Segno MangaGamer profile update

As of the 26 November 2026 release, **the MangaGamer edition of D.S. -Dal Segno- with the official English translation** is a supported Arisa32 profile. v0.7.5 combines both Dal Segno handoffs with the latest fixes to EXTRA, CONFIG, audio, and scene rendering.

### Starting the game and navigating menus

- Arisa32 identifies the MangaGamer edition by its game scripts, even if the installation folder has been renamed. D.S. i.F. -Dal Segno- in Future requires its own profile.
- New Game opens the original name screen. On Android, players can enter given and family names with the keyboard, restore Takamura Atsuya, and review the name through the Yes/No confirmation. The confirmed name appears in dialogue and persists across sessions.
- The dialogue toolbar becomes available in New Game and after Load. The three-dot and floating buttons follow the current scene, menu, and textbox visibility. The Upper Menu opens Save, Load, CONFIG, Hide Textbox, and Return to Title.
- Load on the title screen opens the slot list. Back restores the title screen, and Return to Title clears old scene graphics. The date screen hides the toolbar and accepts a background tap to continue the story, including after Auto Load.
- Backlog uses the original layout with five rows per screen, a scrollbar, per-line voice replay, and confirmation before jumping back to the selected line.

### Saves, loading, and progress

- The Dal Segno save screen has 10 pages with Latest, Auto, Quick, and 97 regular slots. Previews show metadata and titles from a table of 280 scenarios. The game manages Latest and Auto.
- Import and export support PC `DSAO##.dat` slots at supported checkpoints. Arisa32 exports include a companion file for additional state. The current format uses names such as `DSAO03.arisa.dat`, and older companions ending in `.dat.arisa` remain readable.
- Save restoration covers sprite positions, accessories, camera, and audio in the scenes tested. Dal Segno companions now use snapshot v15 and can still read v13 and v14.
- Profiles store names, settings, progress, and Music Mode selections. Arisa32 can use progress from PC `system.dat` and recover supported Scenario and CG unlocks from read history and slots in older Arisa32 profiles.

### EXTRA, Music Mode, and CONFIG

- EXTRA opens **Scenario Replay**, **CG Mode**, and **Music Mode**. Scenario and CG tiles follow profile progress. Players can cancel or exit a replay to return to the list. Save, Load, and the Upper Menu are restricted during replay to keep story progress separate.
- Unlocked CGs open in a preview that preserves the image aspect ratio. Tap to view the next variant and use Back to return to the gallery.
- Music Mode includes 42 original tracks, play/stop, restart, previous/next, a scrollbar, a now-playing indicator, and playlist checkboxes. It supports All On/Off, track and playlist repeat, and shuffle.
- CONFIG uses the game's five tabs and 11 sliders. This update connects text and Auto speed, skip and effect options, textbox transparency, and volume/mute controls for master, BGM, SE, voice, characters, and movies. Leaving CONFIG resumes the story scene.

### Sprites, dialogue, and effects

- Character sprites and accessories use placement and pivots from the PC game, including pan and zoom. Dialogue portraits such as Ame's are visible again, and animated numbers follow Himari and Noeri's sprites. Numbers appear in scenes that call for them.
- Start Game and Return to Title use the shrinking-circle transition. The opening fade, light particles, and background effect layers now render in supported scenes and follow the game's effect settings.
- The date screen uses the original transparent palette and position. English text uses the original game font when available. Inline dash and music-note markers render as images in dialogue and backlog.
- The update corrects the BGM stop command and separates portrait commands from voice playback. Backlog and replay use the voice associated with the current line.

### Test coverage

Desktop and Android checks cover menus, names, saves, backlog, the date screen, camera, and selected effect frames. Full routes, endings, unlock sequences, and frame-by-frame parity with PC still require validation. PC save import at choices and export during movies or timed waits remain limited. [First handoff](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.5/docs/dalsegno-progress-2026-11-21.md) · [Second handoff](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.5/docs/dalsegno-handoff-2026-11-24.md) · [Effects comparison](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.5/docs/dalsegno-effects-validation-2026-11-26.md)

### Android downloads

Both APKs use versionCode 23 and support ARM64 and ARMv7 on Android 5.0 or later (API 21+).

| File | Purpose |
| --- | --- |
| [Arisa32-v0.7.5.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.5/Arisa32-v0.7.5.apk) | APK for regular use and in-app updates |
| [Arisa32-v0.7.5-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.5/Arisa32-v0.7.5-debug.apk) | Debug APK for troubleshooting |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.5/SHA256SUMS.txt) | Checksums for both APKs |

Install the update over your existing Arisa32 installation to retain application data. Arisa32 reads assets from your game installation. [v0.7.5 release page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.5)

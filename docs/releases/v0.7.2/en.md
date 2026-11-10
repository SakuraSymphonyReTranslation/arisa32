[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.7.2 - DC2TY Support, Saves in PC Format, and DC2DM Text Fixes

This November 11, 2026 release adds initial support for D.C. II To You ~Da Capo II To You~ (DC2TY), including original menus, voice decoding, date-change artwork, and PC-format saves with scenario titles. The CONFIG preview and backlog in D.C. II Dearest Marriage (DC2DM) are also easier to read with both MS Gothic and Pretendard JP Bold.

### D.C. II To You

- The opening, Title animation, and four-story selector follow the game's flow. The Title background changes from rainy to sunny after completing all four stories. Story selection shows a preview before a second activation starts the story.
- Extra, CG Mode, Scene Replay, Music Mode, and Config use the original scripts and artwork. Importing `system.dat` reads PC progress and initial settings; reopening the game preserves further progress and newer Arisa32 settings.
- The new decoder supports DC2TY's original voice codec, including dialogue and menu announcements. Per-character volume and mute apply to dialogue; muting a character does not silence menu announcements. This codec fix does not require copying the game files again.
- Music Mode shows the selected song title in the top box, keeps it when hover or pages change, and runs Play, Stop, Repeat, and All Play according to the script.
- Scene Replay retains dates, Prologue/Epilogue labels, and scenario borders on hover and press. Empty rows on the last page are no longer buttons. Dialogue choices use the original pressed state and place Latin text within the panel.
- The Config reset confirmation keeps the menu behind it and blocks input to background sliders. Yes performs the original reset without a second confirmation.
- Close-up sprites use their actual bitmap dimensions for positioning and expression selection. CG previews use source artwork, including vertical movement for tall CGs.
- Scene changes handle directional masks, screen-breakup effects, and dissolves. Returning to Title follows the scripted effect and wait; pause holds presentation timing.
- Date-change cards (eyecatches) now display in full and wait for input. All four backgrounds, date numbers, weekdays, and Prologue/Epilogue labels use the original game assets.

### DC2TY saves with scenario titles and the PC format

- Save/Load shows the current scenario title, save date, and selected icon. The title table follows 129 entries from the original executable, including `始まりの始まり` for the start of Hydrangea.
- Quicksave uses `dc2ty00.dat`; the latest manual-save copy uses `dc2ty01.dat`. The 48 manual slots follow PC numbering: the first page uses `dc2ty02.dat` through `dc2ty04.dat`, and the second starts at `dc2ty08.dat`. Gaps in the numbers follow the PC slot layout.
- Each `.dat` uses the native PC structure and has a `.dat.arisa` companion for additional Arisa32 state, including sprite movement, camera state, and BGM and looping sound-effect playback positions. If the PC changes `.dat`, Arisa32 ignores an older companion that no longer matches.
- Existing `.dc4sav` saves remain loadable. Subsequent saves use native filenames; ZIP backups include `.dat.arisa`.

To transfer a slot to PC, copy `.dat` into the PC game's save folder. When moving saves between Arisa32 installations, keep `.dat` and `.dat.arisa` together. Review the PC verification limits below before relying on cross-save.

### DC2DM backlog readability and touch input

- MS Gothic Latin text in the CONFIG preview and backlog is rasterized directly at output resolution. Glyphs are clearer while retaining the original grid, placement, and reveal timing.
- Pretendard JP Bold uses more readable sizes and proportional letter widths in the CONFIG preview and backlog. Wrapping follows glyph widths; speaker names stay within the name area and voice icons remain visible.
- The backlog responds to quick taps and finger movement when selecting a row. Releasing a finger, scrolling, or changing cursor mode retains the selection according to the original menu behavior. Read from here resumes from the first visible row.

### Support limits

- DC2TY support is still initial; complete playthroughs of every route have not been verified. Native DC2TY backlog artwork is not supported yet.
- In isolated emulation, the original PC executable's reader and loader accept the exported save structure and copy its full contents. Full scene restoration and continued play in the PC game have not been verified. Import rejects native configurations that cannot yet be restored: nonzero background/pan offsets, chips on plane 0, or conditional chips. See the [DC2TY save compatibility notes](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.2/docs/dc2ty-save-compatibility.md).
- The companion stores the dialogue voice resource name for replay from the beginning. The current voice playback position is not saved yet.
- Voice tests cover all 7,800 entries in the original archive and an Android decoder probe. Playback through the game menu to the speaker has not been directly tested in this revision. See the [DC2TY voice report](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.2/docs/dc2ty-voice-analysis.md).

### Downloads and installation

[Open the v0.7.2 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.2)

- `Arisa32-v0.7.2.apk`: release package for normal use.
- `Arisa32-v0.7.2-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Both APKs are universal, each containing ARM64 and ARMv7, and support Android 5.0+ (API 21). The in-app updater downloads the universal release package.

Install the update without uninstalling the app to retain existing data. Game files and translation patches are supplied separately.

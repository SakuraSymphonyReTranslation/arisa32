[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.7.3 - DC2PC Music Mode and Scene Replay Fixes

This 14 November 2026 release fixes song titles, the scrollbar, and the pressed appearance of Music Mode rows and Scene Replay cards in D.C. II Plus Communication (DC2PC). The Indonesian and English READMEs also explain how to read earlier dialogue in DC2PC and DC2TY.

### DC2PC Music Mode

- The top box displays the selected song from the 74 original titles. The title remains visible when the pointer moves, the list scrolls, or Stop is pressed.
- Song rows retain the yellow highlight from the original artwork while pressed, so their backgrounds no longer disappear.
- The white/cream scrollbar thumb is now visible and can be dragged to the last song in the catalogue.

These Music Mode changes are specific to DC2PC and use the original game artwork. See the [Music Mode report and evidence](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.3/docs/dc2pc-music-mode-fix.md).

### Scene Replay and confirmation language

- Dates on DC2PC cards remain visible in normal, hover, and pressed states.
- Pressed cards retain the pale-green hover artwork. Pressing a card directly by touch no longer hides its background or date.
- When the Indonesian patch is selected as the game content, the dialog says **Putar Scenario ini?**. Japanese content retains **このシーンを再生しますか？**. The dialog language follows the selected game content/patch, independently of the Arisa32 interface language.

The card artwork fixes are specific to DC2PC. The Indonesian Scene Replay confirmation wording also applies to other profiles that use the shared dialog. See the [report, image comparisons, and checks for both languages](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.3/docs/dc2pc-scene-replay-fix.md).

### Backlog documentation correction

DC2PC and DC2TY do not have a separate native backlog screen or artwork. Press the up arrow in the textbox area to read earlier dialogue; the down arrow moves forward toward the latest dialogue currently displayed. DC2DM has a dedicated backlog interface.

Both READMEs now describe this as the original game design. Game title formatting was also corrected so tildes do not turn text into strikethrough on GitHub. This section corrects the documentation; it does not add a new backlog menu.

### Verification and support limits

- Six desktop regression tests passed, covering the DC2PC, DC2TY, and DC2DM menus, the system menu, text rendering, and confirmation dialogs. Music Mode was checked with all 74 titles and both MS Gothic and Pretendard JP fonts.
- Android native renderer/input probes passed 120 Scene Replay checks and 36 additional checks using Japanese content and the actual Indonesian patch. Five 1280x720 captures matched the desktop renderer with no differing pixels. These probes ran without the Android Activity, so they were not physical gesture tests in the app.
- The fix checks above used development build 0.7.2/code20. The reports record their limits: DC2DM used fixtures, and DC4 asset checks were skipped. These results do not cover every game or device. DC2TY support remains initial; complete playthroughs of every route have not been verified.
- The universal release and debug APK builds for v0.7.3/code21 completed. Signatures, ARM64/ARMv7 ABIs, the Android API 21+ minimum, ARM64 16 KB alignment, and package contents were verified. See the [v0.7.3 package QA report](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.3/docs/release-qa-0.7.3.md).

### Downloads

Both APKs support ARM64 and ARMv7, with Android 5.0 or later (API 21+).

| File | Purpose |
| --- | --- |
| [Arisa32-v0.7.3.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.3/Arisa32-v0.7.3.apk) | Release APK for normal use; selected by the in-app updater. |
| [Arisa32-v0.7.3-debug.apk](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.3/Arisa32-v0.7.3-debug.apk) | Debug APK for investigating issues. |
| [SHA256SUMS.txt](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/download/v0.7.3/SHA256SUMS.txt) | Checksums for both APKs. |

Install the update over the existing installation to retain app data. Game and patch files are provided separately. All download assets are available on the [v0.7.3 release page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.3).

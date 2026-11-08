[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.7.1 - DC2DM Fully Playable and Font Fixes

D.C. II Dearest Marriage is now fully playable through to completion. This November 8, 2026 release fixes DC2DM menu transitions and read history, adds the Pretendard JP Bold font, and improves COMU text and the DC2PC CG gallery. All fixes from 0.7.0 remain included.

### D.C. II Dearest Marriage

- Moving from Title to story selection follows the scripted transition without an extra black gap. The background, logo, and story labels fade together when a story starts; labels no longer remain over black or reappear when an opening is canceled.
- Title BGM continues into story selection without restarting or overlapping. Returning to Title from an opening uses the current video frame for its transition.
- Entering MINI GAME no longer adds a 1.2-second app delay. The original script controls Otome's introduction and entry into rock-paper-scissors or Reversi.
- Existing read-line history loads before PC progress is imported, preventing startup from overwriting it with an empty history.
- Dialogue, speaker names, and choices using Pretendard JP Bold follow the font's glyph size and advances. Latin text is no longer shrunk or forced into fixed spacing that leaves uneven gaps.

### Pretendard JP Bold and COMU text

- Pretendard JP Bold is available in Android's per-game font picker, with its SIL Open Font License 1.1 notices included. MS Gothic remains the default; choosing Pretendard for one game does not change another game's selection.
- Latin COMU text in DC4/DC4PH patches that use padding spaces now follows Pretendard's letter widths. The 16-pixel size, 8-pixel spaces, and two-line layout are preserved.
- The correction uses the font's identity and patch text, so it still applies after renaming a font file or game folder. Sakuramusubi layouts and other fonts retain their respective behavior.

### D.C. II Plus Communication CG gallery

- CG navigation follows the script's order, including movement between thumbnail groups, skipping locked images, and wrapping at the game's defined cycle boundaries.
- Tall CGs use their original initial crop and vertical movement. The first advance starts movement; taps during movement do not skip the image. Once movement finishes, the next advance opens the following CG.
- Previews are rendered from the source image at the requested display size. Cached samples reduce rendering work during movement; the change does not add detail beyond the original artwork.

The Bahasa Indonesia and English READMEs also explain how to separate original game data in `AdvData` from translation patches and select the patch language in the app.

### Testing and limitations

Feature reports record desktop tests and Android probes for [DC2DM menus and read history](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/dc2dm-menu-font-validation.md), [Pretendard layout](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/dc2dm-pretendard-layout-validation.md), [COMU text](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/comu-pretendard-padding-validation.md), and [the DC2PC gallery](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.7.1/docs/dc2pc-cgmode-validation.md). Those results belong to the development builds identified in each report. Checks for the 0.7.1 packages are recorded separately in the [release report](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/main/docs/release-qa-0.7.1.md).

- Some COMU messages in the DC4/DC4PH patches contain explicit line breaks that produce a third row. That row remains clipped by the original two-row area; this release does not rewrite patch text.
- DC2DM global progress is still stored in `system-profile.dat`; exporting it back to `system.dat` is not supported yet. Loading exported slots directly in the PC executable has not been verified.
- Feature tests on a device use ARM64. ARMv7 support has not been tested on a 32-bit device. These targeted checks do not establish pixel-for-pixel parity with PC or cover every patch variant and device.

### Downloads and installation

[Open the v0.7.1 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.1)

- `Arisa32-v0.7.1.apk`: release package for normal use.
- `Arisa32-v0.7.1-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Both APKs are universal, each containing ARM64 and ARMv7, and support Android 5.0+ (API 21).

The in-app updater downloads the universal release package.

Install the update without uninstalling the app to retain existing data. Game files and translation patches are supplied separately.

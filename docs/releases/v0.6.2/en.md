[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.2 - DC2DM Compatibility Beta

### D.C. II Dearest Marriage: compatibility beta

D.C. II Dearest Marriage (DC2DM) support is still in compatibility development and beta testing. User testing reports that the full game is playable, but support is not yet fully stable.

- Arisa32 now recognizes the DC2DM profile and scripts, with support for the native menus, dialogue, choices, backlog, and portable Arisa32 Save/Load.
- Fixes to sprite positioning, pose changes, layer order, and show/hide triggers. Accessories such as hats follow character position and zoom; removing a character also removes its accessories.
- Scenario Mode / Scene Replay blocks the side menu, Save/Load, and Quick Save/Load, including shortcuts and confirmation dialogs. QS/QL buttons use their disabled appearance; these features remain available during normal story play.
- Opening playback prioritizes `.mpg` files in the MOVIE folder, with `.mp5` as a fallback. The Android FFmpeg backend detects file contents, including patched `.mpg` files that actually contain MP4/H.264 video and AAC audio.
- The native DC2DM ending displays credits and a montage from PCK assets, including normal completion and replay flows. The side menu cannot be opened during the ending.

### Known DC2DM bugs

- Menu fonts are still too large or appear zoomed in.
- Notification popup text colors are still incorrect.
- Character voices often repeat.
- Other UI and compatibility bugs remain during beta testing. Full playability does not mean every bug has been fixed.

### D.C.4, Plus Harmony, and video playback

- The Sakuramusubi English patch for D.C.4 Plus Harmony still uses CP932. Arisa32 now handles its special spacing markers so they do not appear as boxes and keeps Japanese opening quotation marks with the start of a sentence. No new encoding selection is needed.
- The ending after the D.C. segment in D.C.4 Plus Harmony now accepts the 721-pixel-high `GRP/ED/ED.PCK` credits page, continues scrolling, and returns to the main menu.
- Android video completion now lets the decoder process remaining video frames and audio before reporting completion, including after pausing near the end.
- `KotoRI_uta02.mpg` and `dc4_mv001.mpg` from D.C.4 / Plus Harmony played through to completion with audio in Android testing.

### Validation and limitations

- The 0.6.2 recheck passed 22/22 desktop CTest suites, 317 Android game-profile checks, and 253 save-export checks.
- The 0.6.2 release APK library also passed nine replay cases, CP932 spacing checks, and Android video/audio probes. Both APKs passed version, architecture, and signature checks. See the [0.6.2 release verification](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.2/docs/release-qa-0.6.2.md).
- Earlier sprite, credits, ending, and compatibility tests are recorded in the [compatibility report](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.2/docs/dc2dm-dc4-compatibility-validation.md), together with their testing limits.
- These Android probes do not cover the full Activity lifecycle or every game route. Importing native PC DC2DM saves is not supported; Save/Load uses the portable Arisa32 format. Mapping of all backlog portraits has not been verified.
- The desktop video backend still supports MPEG-1. DC2DM patches containing MP4/H.264 require the Android FFmpeg backend.
- The APK supports Android 5.0+ on `arm64-v8a` and `armeabi-v7a`. Game files and translation patches are supplied separately.

### Downloads

[Open the v0.6.2 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.2)

- `Arisa32-v0.6.2.apk`: release package for regular use.
- `Arisa32-v0.6.2-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

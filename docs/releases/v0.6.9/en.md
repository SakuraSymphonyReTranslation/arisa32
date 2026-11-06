[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.9 - Sprite Animation, D.C. Progression, and the CG Gallery

Version 0.6.9 fixes sprite animation and D.C. unlock requirements in D.C.4 and D.C.4 Plus Harmony. It also restores dialogue portraits, improves CG previews, and adds an Android cursor mode.

### DC4 and DC4 Plus Harmony sprite animation

- Movement uses the targets, directions, durations, and command order from the original scripts. Waves, waits, sprite changes, entrances, and exits follow scene timing and text speed.
- The fix applies to the shared sprite system used by other characters, including Nino and Sorane.
- New saves retain active movement, queued motion, and unfinished entrances and exits. Older saves remain readable, but lack the full animation information that this version starts recording.

### D.C. segment requirements

- In both games, the D.C. menu requires the Nino, Sorane, Hiyori, Shiina, Miu, Chiyoko, Arisa, and Arisu endings, together with the game's original unlock marker.
- A profile with a prematurely active D.C. marker remains locked if any of the eight endings is missing. Completing only Nino and Hiyori while Arisu is still in progress does not open D.C.
- Checks apply when reading profiles, importing PC progress, loading older saves, and confirming entry into D.C. Story position and local choices continue to follow the loaded slot.
- Older profiles that incorrectly mark all eight endings complete are not automatically reset because those markers cannot be distinguished from genuine completion.

### Dialogue portraits

- Restores DC4/DC4PH textbox portraits, including Kasumi, using the crops, positions, and accessories from the original FACE assets.
- Portraits follow dialogue changes and background commands, stay on the textbox layer, and support state restoration in new saves. Restoration from older saves depends on the information still available.

### CG gallery

- Previews show the whole image without cropping; portrait images have space on the left and right.
- Android renders previews from the source image onto a canvas sized for the display, preserving detail that was previously lost by first reducing to 720p. Fit to Screen and Stretch still apply.
- Image order follows the gallery scripts, skips locked entries, wraps after the last image, and uses the original transition masks. A tap during a transition completes it before advancing to another image.
- Back to gallery and Android Back return to the CG list. Preview controls share the four-second auto-hide behavior and support keyboard input.

### Android cursor mode

- Cursor mode is available per game in settings and the in-game menu, with Off as the default. Slide to move the cursor, tap without moving to click, or hold then slide to drag.
- The cursor stays inside the game canvas, follows Fit/Stretch, and hides after four seconds without activity. Android dialogs and controls continue to accept direct touch.
- The setting is available in Indonesian, English, and Japanese.

### Game folders and music lists

- DC4/DC4PH base-folder checks detect missing or invalid NAMES.DAT files. Scans skip incomplete folders; registration prevents partial patch folders from replacing complete base data, and launch messages explain the required files. Translation overlays can still contain partial data.
- Locked music rows show `??????` instead of empty titles. Tracks remain locked until the game's requirements are met.

### Testing and limitations

Feature testing covers sprite, effect, portrait, menu, progression, and save-restoration regressions, plus scripts and assets from both games. Previous device checks include 209 cursor checks, 810 CG gallery checks, and 64 game-folder checks on Android 16/API 36. Final package testing is recorded in the [0.6.9 verification report](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.9/docs/release-qa-0.6.9.md).

Direct animation comparisons against recordings cover Nino and Sorane; this is not verification of every animation or a full playthrough of every route. Some sprite fades with per-pixel masks remain approximations. Exact PC parity for all gallery transition timings and the music marker text has not been established. DC2 Dearest Marriage support remains beta. Game files, translation patches, and custom fonts are provided separately.

All 0.6.8 fixes remain included, including full screen, COMU cleanup, and gear transitions. See the [0.6.8 notes](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.8/docs/releases/v0.6.8/en.md).

### Downloads and installation

[Open the v0.6.9 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.9)

- `Arisa32-v0.6.9.apk`: release package for regular use.
- `Arisa32-v0.6.9-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Install the update without uninstalling the app to retain data. In-app updates are available since 0.6.4; Android still requests installation confirmation. Android 5.0+; arm64-v8a and armeabi-v7a.

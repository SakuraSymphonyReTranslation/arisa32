[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.3 - DC2DM Fixes and Game Navigation

Version 0.6.3 brings fixes for D.C. II Dearest Marriage, D.C.4 and Plus Harmony confirmations, image smoothing, and game controls that can be hidden and moved.

### D.C. II Dearest Marriage

- Confirmation popups use text from the game's original images, preserving the PC caption artwork and colors even when the app language changes.
- The CONFIG text preview uses the correct text size and spacing, reveals characters according to the text-speed setting, and displays translation-patch spaces correctly instead of replacement underscores. The transparency setting now works in the correct direction for both the preview and dialogue window.
- Fixed dialogue glyph sizes and spacing, speaker-name placement, and date-image positioning and resets.
- The backlog follows the original four-row layout, colors, button regions, and portrait atlas. Yuto now has the appropriate facelog portrait; the empty pink circle belongs to Yoshiyuki.
- Dialogue-window portraits load from the correct FACE archive, including Yuto and Maya's expression with glasses. Portrait positioning, cropping, clearing, and restoration from new saves have been fixed.
- Voice preload commands no longer play voices early. Unvoiced dialogue no longer inherits the previous line's voice recording for backlog replay.
- Back / Sidemenu is available during normal story play. Scene Replay still restricts the sidebar and Save/Load.

These compatibility fixes apply specifically to the DC2DM profile. Its support remains in beta.

### D.C.4 and D.C.4 Plus Harmony confirmations

- The CONFIG Confirmations On/Off settings now govern the corresponding dialogs, including Save/Load, quick save/load, return to title, replay, and quit.
- The option to stop asking is saved immediately. Re-enabling a confirmation in CONFIG makes the dialog appear again; All On, All Off, and reset continue to follow the game's native settings.

### Floating controls and auto-hide

1. The three-dot button and Back / Sidemenu start in the top-left corner when a game opens, respecting the screen's safe area and camera cutout. Old positions that placed them too far inward are no longer restored.
2. **Auto-hide game controls** hides the controls after four seconds. Swipe the dots or Back / Sidemenu left to hide them manually.
3. While controls are hidden, drag the left-edge handle up or down, for example to move it away from a Game Booster button. The handle's position is saved and adapts to the available screen height.
4. Tap the handle or swipe it right to restore the controls. Vertical dragging only moves the handle, without revealing the controls or forwarding touches to the game.
5. DC4/DC4PH hide and restore the three-dot button. DC2 Plus Communication/DC2 Dearest Marriage hide and restore both the dots and Back / Sidemenu together. Visible controls can still be moved during the current game session.

### Image smoothing and FPS Meter

- Image smoothing is available for every game profile: **Off**, **Bilinear** by default, and **FXAA** on Android 13 or later. The selection is saved and can be changed during play.
- The filter applies to rendered game frames, including backlog, popups, menus, and story scenes. FXAA falls back to Bilinear when the required GPU support is unavailable. MediaPlayer video and Android interface controls use their own rendering paths.
- FXAA softens some image edges; it does not modify texture files, add detail, or guarantee the same result for every image. Some text may appear softer. FPS and battery impact have not been measured.
- Removed the duplicate FPS Meter settings row so only one entry appears.

### Limitations

- DC2DM remains in beta. Testing covers the recorded scenes and cases, not every route, device, or translation patch. Android font rasterization can still differ from the PC version.
- Older saves that did not record a portrait cannot restore an absent image; re-enter the scene to load it. New saves retain the portrait.
- Native PC DC2DM save import remains unsupported; use Arisa32 portable saves. Desktop video remains MPEG-1 only, while patches containing MP4/H.264 require Android FFmpeg.
- Handle positioning is manual; Arisa32 does not reposition Game Booster overlays.

### Validation

This series of fixes has passed 22 desktop CTest suites, 431 Android navigation checks, and 187 image-smoothing/FPS checks. Navigation tests use the real Activity controls in synthetic game areas at several sizes on an I2219/API 36. These counts come from separate test runs; they do not represent complete playthroughs of every game.

See [0.6.3 package verification](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.3/docs/release-qa-0.6.3.md), [DC2DM fix details](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.3/docs/dc2dm-fixes-validation.md), and [confirmations and navigation](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.3/docs/dc4-confirmations-navigation.md).

### Downloads

[Open the v0.6.3 downloads page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.3)

- `Arisa32-v0.6.3.apk`: release package for normal use.
- `Arisa32-v0.6.3-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Android 5.0+; arm64-v8a and armeabi-v7a. Game files and translation patches are supplied separately.

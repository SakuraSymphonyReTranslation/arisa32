[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.4 - DC4 Route and Sakura Fixes, In-App Updates

Version 0.6.4 fixes route branching, sakura effects, and several sprites in D.C.4 and D.C.4 Plus Harmony. Arisa32 can also download updates inside the app with a progress bar, then open Android's installation confirmation.

### DC4 and DC4PH route branching

- Map choices now determine the heroine route group through a previously unhandled scenario command. Selection uses the highest score and the original ordering for ties, without clearing choice scores.
- Restoring a save from before the branch preserves choice results. If a save is already on the wrong route, load a save from before the route-group split and continue from there; the update does not move that save to another route automatically.

### Sakura petals and transitions

- Restored moving sakura petals within scenes and transition effects that use the game's original atlases and masks, including the white portions of the petals and their transparency.
- Durations follow script parameters and the timing rules checked against the PC version. Small petals can keep moving after the main transition wipe ends; effects are not accelerated to replace the screen immediately.
- New saves retain effect state. Older saves remain readable, but saves that did not record active petals need to replay the part of the scenario that starts the effect.

### Nino's zoom and the pocket-watch sprite

- Fixed the zoom center in the two reported Nino frames. The correction applies to the verified DC4/DC4PH loose-CRX image path, resolving zero axis anchors to the image midpoint.
- The pocket-watch sprite that appears after sound effects now follows the correct show/hide command, including when the scenario has just loaded another effect sprite.
- These fixes use commands and image data, so they do not depend on Japanese, Indonesian, or English dialogue. Eight control frames from other zoom scenes remained identical in testing.

### In-app updates

- Automatic or manual checks offer a new version. Tap **Update now** to download inside Arisa32, with a progress bar, download counts, cancellation, and retry.
- The app checks download size and checksum when available, package identity, a newer version, and the signing certificate before preparing installation.
- Downloads use private cache. Arisa32 deletes the temporary APK after handing it to an Android installation session, and cleans up failed or cancelled downloads. No APK is saved to the Downloads folder.
- A session ready for installation can continue after the Arisa32 process stops, including when a phone closes the app as installation permission changes. Downloads still in progress cannot resume after process death; start the download again.
- Android still shows its installation confirmation and, if permission is missing, the **Install unknown apps** settings page. The general warning on that permission page belongs to Android and cannot be removed by Arisa32. Later updates go straight to confirmation once permission is granted.
- Downloading does not open a browser or another download manager. **View Release** remains available for opening the GitHub page.

The downloader is available starting with 0.6.4. Installing 0.6.4 from an older version still follows that installed version's update flow. Install an APK with the same signing certificate without uninstalling the app to retain data.

### Validation and limitations

Testing covers 1,348 map-choice paths from DC4/DC4PH data, including save/restore, plus the recorded effect and sprite checks. Update testing covers 84 host download assertions, 162 release-dialog assertions, 61 device assertions through installation staging, 8 recovery/cancellation assertions, and a real installation in a separate QA app from version code 10 to 11.

- This is not a complete playthrough of every route, patch, or device. The DC4/DC4PH compatibility fixes above do not broadly change other game profiles.
- Effect timing follows the checked rules, but random petal trajectories need not match the PC. This release does not guarantee frame-by-frame equivalence or 60 FPS. Other unmapped effects remain outside the scope.
- The DC2DM fixes, image smoothing, confirmations, and navigation from 0.6.3 remain available. DC2DM support remains in beta.
- Installation testing used Android 16. Minimum-SDK compatibility is checked during compilation; older Android devices have not been physically tested for this updater.

See [0.6.4 package verification](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/release-qa-0.6.4.md), [routes](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/dc4-route-branching-validation.md), [sakura](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/dc4-sakura-validation.md), [sprites](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/dc4-sprite-validation.md), and [in-app updates](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.4/docs/in-app-updates.md) for coverage and results.

### Downloads

[Open the v0.6.4 downloads page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.4)

- `Arisa32-v0.6.4.apk`: release package for normal use.
- `Arisa32-v0.6.4-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Android 5.0+; arm64-v8a and armeabi-v7a. Game files and translation patches are supplied separately.

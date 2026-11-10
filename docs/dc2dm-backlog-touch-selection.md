# DC2DM backlog touch selection

Verified on 2026-11-10 with the iQOO I2219, Android API 36.

## Behavior

- With cursor OFF, opening the backlog selects the first visible row. A tap or finger movement can then select any visible row. Releasing the finger retains that selection.
- With cursor ON, selection follows the cursor and persists when it leaves the row. Opening the backlog does not force the first row.
- Scrolling and live cursor-mode changes preserve the selected screen slot unless the pointer moves over another row. Scrolling does not automatically select the first row.
- The native **Baca dari sini** button resumes from the first visible row. Activating that button selects its target, including when its confirmation is subsequently cancelled.

## Cause and repair

Previously, DC2DM selection changed only during drawing. Android consumes queued DOWN, MOVE and UP events before drawing the next frame. A short tap could therefore set and clear the pointer coordinates without the renderer ever seeing the touched row. The old selection remained white. Row three was not a fixed selection index.

The same phone reproduced the problem before the repair: a quick tap on the first row left the lower row selected; holding the same position for 700 ms selected the first row.

`LogScreen::select_at` now commits row selection from input, including the final release position after a drag. Drawing continues to support native mouse hover. `select_top` supplies the direct-touch opening default and the native resume-button target. Android communicates cursor mode through the existing ordered input queue, so old cursor events cannot run after a mode change and replace its state.

The existing native artwork, row geometry, MS Gothic rendering, palette, voice playback and resume target are retained. The static hand beside the first row and the freely movable white text are separate native controls, as documented in `dc2dm-native-backlog.md`.

## Verification

- PASS: `log_screen_test.exe C:\Windows\Fonts\msgothic.ttc`. Every row among 1–4 visible rows can be selected before drawing; offscreen input preserves selection; explicit top selection works after scrolling and empty/refill; other game rendering is unchanged.
- PASS: Android `backlog-input` probe using the exact packaged arm64 library and isolated save copies. DOWN→UP→frame and DOWN→frame→UP→frame yield identical final pixels. Covered finger movement, a release on a different row, reopening, both arrow sizes, swipe, scrollbar, both cursor modes, mode changes, resume confirmation cancellation and history clearing.
- PASS: Android `text-render` probe. CONFIG reveal beginning/middle/full/hold/restart and backlog normal/selected/scrolled rendering remain functional.
- PASS: `assembleDebug` for arm64-v8a and armeabi-v7a; `git diff --check`.
- PASS: `adb install -r` on serial `10DF1N05QN000H8`. The installed APK hash matches the tested APK. All 5 private files and 9 external save/config files had identical hashes immediately before and after installation, before relaunch.
- The application was relaunched and DC2DM reopened remotely. The user confirmed **“sudah aman”** after testing the installed update.

APK version: 0.7.1 / code 19.

APK SHA256: `98edf3708cb963e6dbc8d1ca84ec4e767b09a6a5b1732b2d704dc17c69dd997c`.

arm64 `libarisa32.so` SHA256: `0be81a860964f1ca22743a08a65e4c51f59416498c90441175c59518ee0752df`.

Output: `android/app/build/outputs/apk/debug/app-debug.apk`.

Temporary evidence:

- Before-repair phone captures: `%TEMP%\arisa32-backlog-selection-uajielmd`.
- Exact-APK probe results: `%TEMP%\arisa32-dc2dm-save-7nrdwo3l`.
- Preinstall app-data backup, save manifests and install verification: `%TEMP%\arisa32-backlog-install-1ip95t_i`.

## Antislop delivery gate

This is an interaction repair within the user-requested native game presentation. No new visual design or copy was introduced.

- Hard gate PASS for the changed interaction: both desktop and Android checks passed, including real touch without a cursor and the installed APK. Text rendering regression checks passed.
- Purpose gate PASS: the opening default communicates the top-row resume target; later selection follows explicit user input. The fixed native artwork and font follow the user's existing direction.
- Liveliness gate PASS within the existing presentation: layout, spacing, color and motion are unchanged; no redesign was introduced.
- Craftsmanship gate PASS: input is independent of render timing, both cursor modes remain usable, and the delivery claims above cite test or installation evidence.

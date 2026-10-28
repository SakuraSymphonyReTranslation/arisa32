# DC2PC recording comparison and fixes — 27 October 2026

Scope: the DC2PC profile. Existing DC4/Plus Harmony behavior and the earlier
sprite, music-catalogue, replay-save, and FFmpeg changes are retained.

## Recording evidence

Sources are the user's `Downloads/dc2pcwindows.mp4` (193.93 s) and
`Downloads/dc2pcandroid.mp4` (132.20 s). Analysis files, contact sheets,
decoded system-voice templates and waveform matches are in `build/dc2pc-av/`.
Audio was matched numerically against the original VOICE.DAT samples;
these findings do not rely on speech transcription or subjective listening.
Android's audio stream starts 0.254208 s after its video stream; audio-match
times below refer to the extracted audio stream, so add that offset for video.

- Returning from Music on Android starts `sys_otm_09` (Interview) at
  25.35012 s, then `sys_otm_05` (Extra) at 25.82006 s. The wrong sample's
  first 150 ms of voiced content correlates at 0.98303 with the original;
  the correct complete Extra sample correlates at 0.97321. This is an
  interrupted wrong announcement followed by the intended announcement.
- Windows opens Load with `sys_otm_04` at 162.86031 s (correlation 0.97452).
  The previously missing host Load-menu mapping therefore has an authored
  source; the recording supports the user's recollection.
- Measuring the game canvas at 30 fps finds nearly black Windows intervals
  at 125.667–127.300, 131.500–132.833, 169.300–171.233 and
  183.300–184.467 s. The Android canvas has no corresponding full-black
  interval. Windows also visibly uses the screen-break effect.
- The Android screenshot follows Music → Extra → Scene Replay. Windows
  shows backgrounds behind all ten rows. The decoded `system/sce_chip02`
  artwork and the authored normal/hover chips contain those backgrounds.

## Changes

- DC2PC raw `3D` registers a button sound; it is no longer played while
  rebuilding a menu. Raw `36` remains immediate voice playback, preserving
  character-voice auditions. Each host continues to own page announcements.
- Load uses native event 04 and ordinary Start uses 03. The separate `dc`
  and `alfine` buttons retain 11/12. Selecting a replay does not inject a
  normal-story start announcement. No unverified Save announcement is added.
- Native layout clearing invalidates the cached catalogue artwork boundary.
  Entering Scene Replay after Music can no longer truncate its normal-row
  backgrounds. Hover, route changes and scrolling use the rebuilt layout.
- DC2PC raw `0D` and `12` now provide displayed/pending frame separation,
  script-blocking fades and native waits. Fades use 16 brightness levels at
  a 40 ms cadence, with speed from bank `99[12]`: 640/320/160/80 ms. Black
  persists during a background change or wait and releases when the script
  publishes a new scene, dialogue or choice.
- Android holds the outgoing frame during Start/return transitions, switches
  behind black, and advances neither input nor dialogue during that boundary.
  Pausing freezes its clock. Title Start preserves the native 1,200 ms wait;
  return preserves 1,000 ms; `sc_end` preserves its 600 ms wait.
- Load validates the destination scene and decodes its audio into an isolated,
  silent mixer. A rejected save leaves the current scene and sound untouched.
  On success, the Android host fades the displayed slot screen to black,
  installs the prepared scene/audio together, then reveals the restored scene.
  Mixer adoption preserves the user's volume settings. The host presents at
  least one complete black frame, including immediate-dialogue destinations.
- Invisible story-toolbar controls cannot be activated during DC2PC fades,
  black holds or waits. Consecutive waits with the native timer-reference flag
  advance the reference clock instead of collapsing into a single delay.

## Validation

| Check | Result |
| --- | --- |
| Final desktop build | Passed |
| Final Android debug / release build | Passed for arm64-v8a and armeabi-v7a (11 min 9 s) |
| APK signatures, alignment and native payloads | Passed: both signatures, 16 KiB alignment checks, ZIP integrity, and packaged libraries match final stripped outputs |
| Final CTest suite | Passed, 18/18 targets (26.00 s) |
| Actual DC2PC menu suite | Passed: announcements, complete catalogue frames after Music, all ten hovered rows, routes, scrolling, replay restrictions, settings and persistence |
| Actual DC2PC flow suite | Passed: ordinary story continuation, replay ending/catalogue return and four route branches |
| Actual DC4 / Plus Harmony music catalogues | Passed: 42 / 43 rows and playback/navigation checks |
| ARM64 device transition test | Passed on I2219: four native speeds, waits, black boundary, reveal and frozen-clock rendering |
| Deferred mixer regression | Passed: preparation preserves outgoing sound, playback clocks remain frozen, adoption preserves samples, ramps and user volume controls |

Logs are `build/dc2pc-av/final-ctest.log`, `final-dc2pc-menu.log`,
`final-dc2pc-flow.log`, `final-dc4-music.log`, `final-dc4ph-music.log` and
`arm64-transition-test.log`. The ARM64 test is a standalone native executable
under `/data/local/tmp/arisa32-dc2pc-transition-probe`, not an interactive APK
test. It does not open the game or play audio. The corrected replay image is
`build/dc2pc-av/captures/scene-after-music.png`.

Original game assets and saves were read only. Test saves used temporary
fixtures beneath the build directory. The previously validated sprite and
FFmpeg changes remain included; see `docs/menu-sprite-validation.md` and
`docs/ffmpeg-validation.md` for their earlier, separate checks.

The native mode-14 screen-break animation currently uses a plain black fade
fallback. Its original tile/particle pattern and the full title-introduction
sequence are not reproduced by the host boundary animation.
The native effect uses 475 moving 32-pixel tiles and `system/eff.CRX`; its
randomized movement and variation selection remain unimplemented. The black
fade is the native alternative when the screen-break setting is disabled.

## Delivery and remaining device checks

The final release APK was installed with `adb install -r` on I2219 ARM64
without clearing app data. The device reported package `org.dc4engine`, version
0.6.0 / code 7, updated on 27 October 2026 at 16:54:50. Microsoft Edge remained
foreground before and after installation. No controlled in-app visual/speaker
or process-recreation test was performed after this update. The 32-bit ABI was
built and packaged but not run on a 32-bit device.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `android/Arisa32-debug.apk` (also `android/dc4-debug.apk`) | 52,194,142 | `72329842b27948c8de04db5d464e03108f8cbc42f76d85fbeef7baf9be8e0de7` |
| `android/app/build/outputs/apk/release/app-release.apk` | 13,136,885 | `a4406651b4b5a0aa202f6056c0f456a465cb71fff275522a2eab4a787e806dca` |

The release retains the project's existing debug signing configuration.
Package verification is recorded in `build/dc2pc-av/final-apk-verification.json`
and `apk-signature-alignment.log`; installation is recorded in
`device-install.log`. AGP/compileSdk, Java native-access and unsigned META-INF
metadata warnings remain nonfatal; both APK signature checks pass.

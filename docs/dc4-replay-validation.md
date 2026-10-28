# DC4 and DC4 Plus Harmony replay controls and Movie return

Scope: DC4 and DC4 Plus Harmony. The reference recordings are analyzed in
`build/dc4-replay-validation/video-evidence.md`. They show Android opening Save
and Load from Scenario Mode, while the requested Windows behavior keeps those
controls visible but inactive.

## Changes

- Scenario Mode disables Quick Save, Quick Load, Save and Load using the native
  replay variable. The existing toolbar artwork remains visible, without its
  active hover state. Disabled taps are consumed silently and do not advance
  dialogue.
- The same policy covers native menu actions, quick confirmations, save/load
  files and Android pause checkpoints. Rejected writes leave existing saves
  and companion files unchanged. Internal rendering/history snapshots remain
  available. Leaving replay restores normal-story persistence.
- Game detection uses script format and scenario fingerprints, rather than
  matching a directory name such as `org.dc4engine`. Other modern game profiles
  retain their behavior; the earlier DC2PC replay policy is preserved.
- Movie Mode retains the authored Music-page return button as an interactive
  control. Its native hover artwork works, and clicking it closes the popup
  while preserving Music state. Other controls beneath the popup stay inactive.

## Verified results

| Check | Result |
| --- | --- |
| Desktop CTest suite | 18/18 passed |
| DC4 real-asset replay test | Four toolbar controls, native hover appearance, storage protection and story restoration passed |
| DC4PH real-asset replay test | Same checks passed |
| DC4 and DC4PH Movie popup tests | Return hit target, native hover artwork and Music state restoration passed |
| Captured menu frames | Visually inspected both games' popup, hovered return, restored Music page and replay toolbar |
| Final Android debug build | Passed for `arm64-v8a` and `armeabi-v7a` |
| Final APK verification | ZIP integrity, v1/v2 signatures and 16 KiB ZIP alignment passed |
| Android ARM64 production JNI probe | 6/6 cases passed: silent replay taps, unchanged dialogue/save/checkpoint bytes, ordinary-story save/load and unrelated-profile isolation |
| JNI negative regression check | The library before the disabled-tap fix reproduces `Not available on Android: :disabled` |

The unchanged desktop build and asset tests were completed in the preceding
chat and their logs and artifact freshness were checked on continuation. The
final Android rebuild additionally fixes the unsupported-action message that
would otherwise appear for `:disabled` taps.

The JNI probe ran the final APK's actual ARM64 library on the connected device
using isolated synthetic fixtures and `app_process`. All four disabled toolbar
taps were consumed without messages or dialogue advancement; replay save/load
and pause-checkpoint writes were blocked without changing existing bytes.
Normal-story and unidentified-game cases retained working save/load. Both
`AAB6` and `ABB6` script formats passed. The tested `libarisa32.so` SHA-256 is
`30f2f293a95982125ed5ac59828d8563ba806bbb81f0cae7c45da04cef866317`, which matches
the library packaged in all three APK copies below. A subsequent harness edit
only added exception reporting; the negative regression check then captured
the old library's expected failure. These fixtures omit artwork and audio.

Evidence is under `build/dc4-replay-validation/`: `current-ctest.log`,
`current-dc4-replay.log`, `current-dc4ph-replay.log`, `current-dc4-movie.log`,
`current-dc4ph-movie.log`, `final-android-build.log` and
`final-apk-verification.json`. Visual contact sheets are in the `current-dc4/`
and `current-dc4ph/` subdirectories. Real game assets were read only; test save
writes used disposable fixtures.

JNI evidence: `jni-probe-d3a4bb6c4ca5/result.log` (6/6 passed) and
`jni-probe-d567223a2b43/result.log` (expected old-library failure), under the
same evidence directory. Reproduce with `python tests/android/run_replay_probe.py`;
setup and scope are documented in `tests/android/README.md`.

## APK

`android/Arisa32-debug.apk`, `android/dc4-debug.apk` and
`android/app/build/outputs/apk/debug/app-debug.apk` are identical:

- Version: 0.6.0 / code 7, package `org.dc4engine`.
- Size: 52,194,142 bytes.
- SHA-256: `7dd26ecb0437984dc509ca693bd01a184cdc699731e6354d23cf73cf4078f383`.
- Each ABI contains `libarisa32.so`, `libc++_shared.so` and five FFmpeg libraries.

The final APK has not replaced the installed app. Android validation used the
ARM64 JNI probe above; an interactive game UI test has not been performed.
The ARM32 payload is built and packaged; it has not been run on a 32-bit device.

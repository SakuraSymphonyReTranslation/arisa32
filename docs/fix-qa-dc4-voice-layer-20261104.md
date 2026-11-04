# DC4 and DC4 Plus Harmony voice and sprite verification

Verified on 2026-11-04. This working build fixes voice replay associations and
scripted sprite priority for D.C.4 and D.C.4 Plus Harmony. APK metadata remains
0.6.6 / version code 13; this is a local fix build, not a published release.

## Changes

- Advancing through the dialogue wait clears the replay reference. Narration
  and unvoiced Ichito lines no longer inherit the preceding character's voice.
  Existing playback may finish according to the configured advance behavior.
- Sprite command `0x8B`, mode `0x20`, sets draw priority in the native 0..100
  range. Default priority is 50; Gen's priority 49 puts him behind Mitsumi.
  Equal priorities preserve the existing draw order.
- Non-default priorities survive hiding, replacement and save/load. Emulator
  snapshots use format 8 when needed; versions 1..7 remain readable. PC save
  import/export carries priority in the native renderer record.
- Progress tests exercise all 256 combinations of the eight ending flags in
  each game's actual `ed.mes`. D.C. requires flags 101..108. Nino and Hiyori
  alone do not unlock it. The existing v0.6.5 progression fix remains in place.

## Results

| Check | Result | Evidence under `build/dc4-voice-layer/` |
| --- | --- | --- |
| Desktop audio, scene and backlog | PASS: voiced/unvoiced line sequence, replay, backlog button pixels, save/load, priority clamping and native saves | `tests.log` |
| Original DC4/DC4PH progression | PASS: 256 combinations per game, two-route profile restart, extras navigation, settings reset and PC import | `tests.log` |
| Additional regressions | PASS: all 6 tests: DC4 effects, DC2PC flow, DC2DM ending/sprites, save screen and desktop save host | `tests-extra.log` |
| Actual release library on Android arm64 | PASS: `audio_test`, `scene_test --dc4-sprites`, `dc4_effect_test`; zero failing exits | `android-native/results.json` and individual logs |
| Mitsumi/Gen original assets | PASS: Mitsumi renders in front in both games | `after-sprites.png`, `after-dc4-sprites.png` |
| Reported Ichito video scene | PASS: Nino has replay buttons; Ichito and narration do not | `after-video-ichito-log.png` |
| Original/corrected phone profile | PASS: original unlocks D.C.; corrected locks it and retains Nino/Hiyori completion and scene records | `tests-extra.log`, `corrected-profile/changes.json` |
| Release build and package | PASS: Gradle build, ZIP integrity, both ABIs, v1/v2 signatures, expected package/version and signing certificate | `android-build-final.log`, `package-verification.json`, `apk-signature.log`, `apk-manifest.log` |

The Android checks load the exact arm64 library extracted from the delivered
APK in an isolated `/data/local/tmp` directory. The tests do not install or
replace the main application. Their temporary device directory was removed.
Synthetic fixtures intentionally omit unrelated menu assets; the corresponding
missing-asset messages are expected. Assertions and process exits passed.

The render probe skips `dc4_nin20190503d.mes` on DC4 All Age because that
Plus Harmony script does not exist there. The shared Mitsumi and backlog
scenes were rendered in both games; the reported video scene was rendered
using Plus Harmony's script.

## APK

File: `build/dc4-voice-layer/Arisa32-DC4-voice-sprite-fix.apk`

- Size: 13,289,486 bytes.
- SHA256: `a6c9f7ea52441b15dd049323121c561c034a62502980217b6fe0a83ee0e4290a`.
- Package: `org.dc4engine`; min SDK 21; target SDK 36.
- ABIs: `arm64-v8a`, `armeabi-v7a`.
- Signing certificate matches the published builds:
  `6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
- Tested arm64 engine SHA256:
  `5f89925802939b7b6ce30592705a0f759c6634d8faa0ca9a975951bd78b48dea`.

The build uses the existing local init script to select tracked AGP 8.5.2.
The pre-existing local `android/build.gradle` edit remains separate.

## Old profile diagnosis

The user confirmed that only Nino and Hiyori were completed and that play
started before v0.6.5. Both the earlier backup and the newly read phone profile
have every flag 100..119 set to 1, including unused flags. Only a non-progress
value changed between the two profile reads. This matches the old automatic
progress initialization bug; a new APK alone cannot determine which of those
old completion flags are legitimate.

The local corrected copy changes 16 one-byte values from 1 to 0, retaining
101 and 103 (Nino/Hiyori), 110 and 112 (ending-related unlocks), and every
other byte, including settings, CG/music/scene history and flag 120.
Save slots and read-line history are untouched. The correction has not been
applied to the phone. Its source hash and exact changes accompany the file.

## Scope and delivery gate

The user's PC references and original game assets supply the visual direction.
This is a compatibility repair; it introduces no new layout, typography,
palette, artwork, motion or product copy. Design-generation dials and unrelated
application-wide visual checks do not apply to this change.

| Gate | Status and evidence |
| --- | --- |
| R-26 / C-2 | PASS: replay tests reject unvoiced lines and retain the correct recording on voiced lines; backlog pixels match those associations. |
| R-31 / C-1 | PASS: sprite priority follows the native command; each retained new comment explains a protocol or regression constraint. |
| R-33 | PASS: changes are in the C++ source and tests; no runtime source rewriting. |
| R-35 / C-4 | PASS within changed scope: desktop tests, original-asset renders, APK build and tests against the delivered library on Android passed. |
| R-02 / R-15 / R-16 | PASS: this report uses direct descriptions and specific artifact names, without marketing claims. |
| R-17 / R-18 / R-23 / R-36 / R-38 / C-5 | PASS: test counts, hashes and observations have local evidence; no generated product assets or fabricated claims. |
| Purpose, liveliness and layout checks | No new design to evaluate; existing game visuals are preserved and the requested PC sprite order is verified. |

These checks are not full route playthroughs or an application-wide
accessibility audit. Android execution was tested on the connected I2219;
armeabi-v7a was built but not executed. Historical backlog or emulator
snapshots already containing an incorrect voice reference are not migrated
by this change. New line advancement records the corrected association.

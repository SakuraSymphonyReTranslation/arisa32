# Arisa32 0.6.2 release verification

Verified on 2026-10-30. This release packages the DC2DM compatibility changes, updates the Android version to 0.6.2 (versionCode 9), and records the user's beta status and known bugs in the changelog and release notes.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.2.apk | 13,218,943 | `4ec9ed93d6b584dd9e2d94b51990c35357d6d0131ae770064d130778a4464d03` |
| Arisa32-v0.6.2-debug.apk | 14,148,201 | `4160ce76d76f704df28b93b8f85ba51cb78fc7cd2335c096e212bc828f243d79` |

Both files match the corresponding Gradle outputs. ZIP integrity checks passed. Each contains the runtime, C++ runtime, and five FFmpeg libraries for arm64-v8a and armeabi-v7a. Manifest inspection confirms package `org.dc4engine`, versionName `0.6.2`, versionCode `9`, minSdk `21`, and targetSdk `36`. Only the debug APK is debuggable.

APK v1 and v2 signature verification passed for both variants. The signing certificate matches the published 0.6.1 package: SHA256 `6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Every native library in the new debug APK is byte-identical to its counterpart in the previously tested `Arisa32-v0.6.1-dc2dm-debug.apk`. The release runtime was tested separately below.

Package evidence: `build/release-0.6.2/packages.json`, `release-signature.log`, `debug-signature.log`, `release-badging.log`, and `debug-badging.log`. Distribution files and `SHA256SUMS.txt` are under `dist-v0.6.2/` locally.

## Fresh build and regression results

| Check | Result | Local evidence |
| --- | --- | --- |
| Desktop Release build | PASS | `build/release-0.6.2/desktop-build.log` |
| Desktop CTest | PASS: 22/22 suites, 0 failures, 110.01 seconds | `build/release-0.6.2/desktop-test.log` |
| Android profile checks | PASS: 317 checks | `build/release-0.6.2/java-profile-test.log` |
| Android export checks | PASS: 253 checks | `build/release-0.6.2/java-export-test.log` |
| Android release and debug build | PASS: both variants and both ABIs, 4 minutes 46 seconds | `build/release-0.6.2-android-build.log` |
| Packaged release library probes | PASS: replay, CP932, and video/audio cases below | `build/release-0.6.2/android-packaged-tests.log` |

The desktop run set `DC2DM_DATA`, `DC4PH_DATA`, and `DC2DM_MENU_FONT` to the installed assets and MS Gothic font. It exercised DC2DM menus, the three route openings, ending, original marker 65 sprite placement/removal, and DC4PH credits. Four optional installed-data sections were skipped: DC2PC flow, DC2PC menu, DC4 system menu, and the music catalogue. Their synthetic test coverage still ran.

The Android probes loaded the release APK's actual arm64 runtime, SHA256 `e8acd862625975ac077dc4c836ac2b8278d20c02f0e3625761b0a74323f888b3`, with no source-backend override. The run manifest is `build/dc2dm-check/opening-ending/packaged-bd2d92a6d269/manifest.json`.

- Nine replay cases passed, including both DC2DM replay flags, ordinary-story controls, and the existing DC4 cases. Replay taps were consumed without advancing dialogue; persistence was blocked; normal-story saves completed a round trip.
- The Android Java CP932 decoder check passed for the custom translation spaces.
- The H.264 test processed all 360 frames and 265,216 audio samples with 200 ms polling.
- The same test processed all frames and samples after a six-second pause at 5.9 seconds, with no decoder error.
- Delayed-audio, silent-tail, and synchronized-control fixtures completed successfully.
- The original `KotoRI_uta02.mpg` and `dc4_mv001.mpg` completed with audio, decoding 420 and 810 frames respectively.
- A separate five-second DC4 sample using the silent-video clock showed a changing, nonblack frame before completion.

## Scope and limits

DC2DM remains a compatibility beta. The user reports full playability and also reports oversized menu fonts, incorrect popup text colors, repeated character voices, and other unresolved bugs. Native PC DC2DM save import is unsupported; the profile uses Arisa32 portable saves. Complete backlog portrait mapping remains unverified.

The probes cover packaged native code and Android decoding services. Full installed Activity lifecycle, every route and UI state, and runtime testing on armv7 remain outside this release check. Earlier sprite, ending, and menu evidence is documented in [the compatibility report](dc2dm-dc4-compatibility-validation.md). Desktop video remains MPEG-1 only.

The existing build emits warnings about AGP 8.5.2 with compileSdk 36, the manifest package attribute, deprecated Java APIs, and Gradle deprecations. They did not fail the build. The older `LibraryChecks` harness has a documented expectation mismatch with the current importer and is outside the passing standalone profile/export results.

## Release copy delivery gate

The primary agent checked the authored release copy against the user's requested beta status, source changes, and recorded test outputs. The copy follows the repository's Indonesian release-note style, places the beta status and known bugs before validation details, and introduces no new visual design. Visual-only gate items do not apply to these documentation and version edits; the earlier interface review has its own scoped evidence in the compatibility report.

| Item | Status and evidence |
| --- | --- |
| R-02 | PASS: new release copy contains no em dash and uses direct factual wording. |
| R-17 | PASS: counts, dates, byte sizes, and hashes come from the recorded commands and package outputs. |
| R-18 | PASS: no testimonials or invented identities. |
| R-23 | PASS: no new visual assets or navigation structure. |
| R-24 | PASS: documentation links point to included files; the release URL names the requested v0.6.2 tag. |
| R-35 | PASS for release scope: desktop and Android builds ran, packages were inspected, and release-library probes passed. |
| R-36 | PASS: full playability is attributed to user testing; known bugs and test limits remain explicit. |
| R-37 | PASS for copy scope: existing release documentation and the user's beta notes supply the direction. |
| R-38 | PASS: feature descriptions correspond to source changes and recorded compatibility evidence. |
| R-15 | PASS: download labels identify the release APK, debug APK, and checksum file. |
| R-16 | PASS: no promotional or unsupported performance claims added. |
| C-1 | PASS: the version, beta note, known bugs, and validation sections each support release decisions. |
| C-3 | PASS: sections cover actual changes, package checks, and limitations. |
| C-5 | PASS: the report separates user playability feedback from automated verification. |

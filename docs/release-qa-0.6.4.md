# Arisa32 0.6.4 release verification

Verification ran on 2026-11-02. This release packages the DC4/DC4PH map-route, sakura, Nino zoom and pocket-watch fixes, plus in-app APK downloads and installation handoff. Android versionName is `0.6.4` and versionCode is `11`.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.4.apk | 13,286,008 | `4f53cada59bce739eb3bd26d96289d279a73f4e5e0b4af199bad3cff07a188e9` |
| Arisa32-v0.6.4-debug.apk | 14,221,541 | `d91b467b383ed3b813c9062e8b16cc77572a5d50077e9c8ab7100ca06c7c29ce` |

Both APKs match the Gradle outputs and pass ZIP integrity checks. Their manifests report package `org.dc4engine`, version 0.6.4/code 11, minSdk 21 and targetSdk 36. Only the debug APK is debuggable. Both pass v1/v2 signature verification and use the same certificate as the verified published 0.6.3 APK: SHA256 `6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Both contain `libarisa32`, the C++ runtime and five FFmpeg libraries for arm64-v8a and armeabi-v7a. DEX checks find the updater, floating-navigation, frame-filter and release-note classes, and reject instrumentation test classes. Package inspection rejects bundled MES, CRX, CRM and Arisa32 save files.

Local evidence under `build/release-0.6.4/` includes `packages.json` with source/library hashes, signature logs for both APKs and the previous release, and both manifest badging logs. Distribution files and `SHA256SUMS.txt` are under `dist-v0.6.4/`. Generated evidence and APKs are excluded from the source commit.

## Build and host regression results

| Check | Result | Local evidence under build/release-0.6.4/ |
| --- | --- | --- |
| Desktop Release build check | PASS: Ninja reported no work to do; incremental build check | `desktop-test.log` |
| Desktop CTest | PASS: 24/24 suites, 0 failures, 99.73 seconds | `desktop-test.log` |
| Update download tests | PASS: 84 assertions across 15 scenarios | `download-test.log` |
| Release-note parser and language documents | PASS: 115 checks, including both 0.6.4 documents | `release-notes-test.log` |
| Android release and debug build | PASS: both variants and both ABIs, 11 minutes 7 seconds | `android-build.log` |

The desktop run supplied installed DC4, DC4PH and DC2DM data through `DC4_DATA`, `DC4PH_DATA` and `DC2DM_DATA`, plus MS Gothic through `DC2DM_MENU_FONT`. Route coverage includes 1,348 installed-data map paths with and without snapshot restoration. Effect checks use both games' original mask and sakura assets. Optional installed-data checks for other profiles remain outside this run's scope.

The Android build used Gradle 9.2.0, Android Studio's JDK, NDK 27.3.13750724 and the repository's tracked AGP 8.5.2. The unrelated local AGP 8.13.2 edit was backed up, temporarily replaced during the build, then restored byte-for-byte and excluded from the release commit. Warnings about compileSdk support, SDK XML versions, the manifest package attribute, deprecated APIs and Gradle deprecations did not fail the build.

## Final release library checks on Android

The arm64 library extracted from `Arisa32-v0.6.4.apk` passed these tests on the connected I2219/Android 16 phone. All three probes report SHA256 `6f5ac423215d6d6f6765c8afbd8abb5dde85312c1324ef27d087076bb5b36493`, matching the release package's library.

| Check | Result | Local evidence under build/release-0.6.4/ |
| --- | --- | --- |
| Production JNI replay and routing | PASS: 9 replay cases, 24 map-route cases, and the Java CP932 translation-space check | `android-replay.log` |
| Sakura effects | PASS: 0 failures, synthetic coverage for both modern MES versions and original DC4 assets | `android-effects.log` |
| Nino zoom and pocket-watch controls | PASS: focused `scene_test --dc4-sprites` regressions | `android-sprites.log` |

These probes run extracted production code in isolated `/data/local/tmp` directories. They do not install or open the main app, use its saved preferences, or change user saves. Missing optional UI resources in the synthetic JNI fixtures produce log messages; all asserted replay, route and text outcomes pass.

The final release effect probe's 43-frame sample averaged 38.38 ms per software-rendered frame, including 51.30 ms during the wipe, 8.55 ms for the remaining flowers, and a 0.62 ms static baseline. This sample excludes Activity rendering and GPU presentation; it is not a 60 FPS result or a general performance comparison.

## Earlier feature validation

The following results precede final versioning and packaging. They are separate feature runs, not a combined interactive test of the final 0.6.4 APK.

| Feature | Result and scope | Evidence |
| --- | --- | --- |
| Map-route selection | All 1,348 installed-data map paths passed, plus later heroine comparison cases. The previous library reproduced the wrong-route defect. | [Route report](dc4-route-branching-validation.md) |
| Sakura effects | Synthetic and original-asset checks cover white petals, source alpha, timing, masks, concurrent waits, small-petal completion, save state and profile isolation. | [Sakura report](dc4-sakura-validation.md) |
| Nino zoom and cut-in sprites | Both Nino frames match the supplied PC framing; eight unrelated control frames are byte-identical. Captures cover the watch's appearance/removal and additional CUT045/CUT049 sequences. | [Sprite report](dc4-sprite-validation.md) |
| Update offer dialog | 162 assertions for EN/ID notes, scrolling, keyboard activation, missing-APK handling, 540dp/380dp widths and 100%/200% text. | `build/updater-validation/` |
| Update staging and installation | 61 device assertions through staging, 8 recovery/cancellation assertions, and an actual QA package upgrade from code 10 to 11. The cache and persisted session map were empty afterward. | [Updater report](in-app-updates.md), `build/updater-validation/` |

Updater tests used the separate `org.dc4engine.updateqa` package on Android 16. Download transport used fixture streams; package parsing, signature checks, staging, Android confirmation and package replacement used the real platform. The QA packages were removed after testing. The init script pins its baseline to 0.6.3/code 10 and candidate to 0.6.4/code 11, independently of future production versions.

## Scope and limits

Coverage is limited to the documented scenes, fixtures and device. It does not represent complete playthroughs of every route, patch or profile. A save already in the wrong route needs a save from before the group split. Old saves without ambient-effect state cannot reconstruct a running emitter until the script starts it again.

Effect timing follows the checked native rules; random trajectories need not match another PC run. Frame-by-frame PC equivalence remains unverified, and there is no 60 FPS claim. The earlier original-asset device sample averaged 40.51 ms per software-rendered frame, excluding GPU presentation and Activity work. Other unmapped effects remain outside this fix. DC2DM support remains in beta.

Android controls installation permission and confirmation. Its generic warning on the Install unknown apps settings page cannot be removed by Arisa32. A prepared installation can recover after process death; an unfinished download must restart. Updater testing on physical older Android devices and native runtime testing on armeabi-v7a remain outside this release check.

## Release copy delivery gate

Antislop mode is `during`, selected earlier in the session. The primary agent checked the new release copy against the implementation, recorded feature results and the established bilingual release format. This release task changes documentation and versioning; it introduces no new interface design or visual assets. Applicable UI evidence remains in the feature reports.

| Item | Status and evidence |
| --- | --- |
| R-02 / R-15 / R-16 | PASS: direct descriptions of the fixes, explicit APK/checksum labels, and no em or en dashes in either new release document. |
| R-17 / R-36 / R-38 | PASS: test counts and limitations come from recorded results; no universal compatibility, silent-install or 60 FPS claims. |
| R-18 / R-23 | PASS: no invented people, testimonials or visual assets. |
| R-24 | PASS for document scope: language selectors identify the included translations; report and download links use the requested tag. Public URLs are checked during publication. |
| R-35 | PASS for release scope: final Android builds, desktop regressions and package verification completed; earlier interactive device runs are distinguished from final package checks. |
| R-37 | PASS for copy scope: the user's requested fixes, two-language requirement and existing release format supply the direction. |
| C-1 / C-3 / C-5 | PASS: both complete translations describe the same changes, upgrade behavior, test scope, limitations and downloads. |

Purpose and liveliness checks add no new visual requirements to these Markdown and version edits. Contrast, focus, progress layout and interaction results are recorded in the updater feature report.

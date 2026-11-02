# Arisa32 0.6.5 release verification

Verification ran on 2026-11-02. This release packages the DC4/DC4PH progression,
title-menu and map-selection fixes. Android versionName is `0.6.5` and
versionCode is `12`.

## Packages and build

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.5.apk | 13,286,717 | `d91f93fd1d9a6a1b76b911433beec740bff1f8a82f6084fe0e0874e22d555fdd` |
| Arisa32-v0.6.5-debug.apk | 14,222,100 | `71739c758c57add608c1b0f2977b7726c037669f03e184cb57cec1e62e3858fa` |

Both APKs match the Gradle outputs and pass ZIP integrity and v1/v2 signature
verification. Their manifests report `org.dc4engine`, version 0.6.5/code 12,
minSdk 21 and targetSdk 36. Only the debug APK is debuggable. Both use the same
certificate as the verified published 0.6.4 APK: SHA256
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Package checks confirm both ABIs, the engine/C++/FFmpeg libraries, and the
existing updater and navigation classes. They reject instrumentation classes
and bundled MES, CRX, CRM or Arisa32 save files. Package hashes and source
hashes are recorded in `build/release-0.6.5/packages.json`; signature and
manifest logs are in the same directory. APKs and `SHA256SUMS.txt` are in
`dist-v0.6.5/`. Generated evidence and packages are excluded from the commit.

The release/debug build passed in 12 minutes 16 seconds using Gradle 9.2.0,
Android Studio's JDK, NDK 27.3.13750724 and tracked AGP 8.5.2. An init-script
plugin override kept the unrelated local AGP 8.13.2 edit intact and outside the
commit. Existing SDK, manifest-package and deprecation warnings did not fail
the build. Evidence: `build/release-0.6.5/android-build.log`.

The final release `libarisa32.so` files are byte-identical to the feature-tested
APK for both ABIs. The tested arm64 library has SHA256
`dd37a467817ddfc4a020cc30dc57ef53ff28739b573bffa3d9c403929aa105c7`;
armeabi-v7a has
`fe66dec5b4ecb50094d9a43f81068fc6135776c76e67b68c5c7044e227a22375`.
All recorded production-source hashes remained unchanged after packaging.

## Regression results

The production C++ changes were tested before the version bump. No production
runtime code changed between those checks and release packaging.

| Check | Result | Local evidence |
| --- | --- | --- |
| Desktop CTest | PASS: 24/24 suites, 0 failures, 52.64 seconds | `build/dc4-progression/full-check.log` |
| Final map and progression checks | PASS: 1,348 map paths, 24 heroine comparisons, all ending/prerequisite checks, profile lifecycle, and 16 map renders with touch-target checks | `build/dc4-progression/final-captures.log` |
| Android production JNI | PASS: 4 locked-progression cases, 24 map-route cases, 9 replay cases, and CP932 spaces | `build/dc4-progression/android-jni.log` |
| Release-note parser and translations | PASS: 123 checks, including both 0.6.5 documents | `build/release-0.6.5/release-notes-test.log` |

Desktop tests used installed DC4, DC4PH and DC2DM data. Locked/unlocked title
screens and all four maps were visually inspected for both DC4 versions. The
extra character and location marker appear only in the unlocked map artwork,
together with the fourth choice. Original scripts supply the artwork and
conditions; this release adds no substitute map images or custom route rules.

The Android JNI probe ran in an isolated directory on an I2219/Android 16
device. It did not install the main app or use user saves. Missing optional UI
resources in the synthetic fixtures produce log messages; all asserted
progression, routing, replay and text outcomes pass.

## Scope and upgrade behavior

Earned global progress survives save loading, backlog rewind, configuration
reset and profile import ordering. Map scores stay in the restored scene.
Fully completed profiles keep their unlocks. The tests cover fresh, partial
and completed profile states, including stale save snapshots and PC imports.

Older profiles may contain completion flags fabricated by the former bug.
Those values cannot be distinguished from earned progress and are not reset
automatically. See [DC4 progression](dc4-progression.md) for the exact conditions
and recovery limitation.

These checks do not represent complete route playthroughs, every patch or
every device. Visual captures use desktop rendering and original assets;
Android runtime checks use synthetic scripts through production JNI.
Native armeabi-v7a execution and physical older Android devices were not tested.
DC2DM support remains beta.

## Release copy delivery gate

Antislop mode is `during`, selected earlier in the session. The requested
fixes, bilingual requirement and existing release format supply the direction.
This release task changes versioning and documentation and introduces no new
interface design or visual assets.

| Item | Status and evidence |
| --- | --- |
| R-02 / R-15 / R-16 | PASS: direct descriptions, explicit download labels, no em/en dashes or marketing filler in the new release copy. |
| R-17 / R-36 / R-38 | PASS: counts come from recorded tests; legacy-profile and device limits are explicit. |
| R-18 / R-23 | PASS: no invented people, testimonials or visual assets. |
| R-24 | PASS for document scope: both language selectors target the supplied translations; report and download links use v0.6.5. |
| R-35 | PASS for release scope: final Android builds and package verification passed; desktop and Android regression evidence above identifies the visual and JNI test scopes. |
| R-37 / C-1 / C-3 / C-5 | PASS: complete translations describe the same changes, upgrade behavior, test scope, limitations and downloads. |

Purpose and liveliness checks add no visual requirements to these Markdown and
version edits. Map and title rendering evidence belongs to the feature checks.

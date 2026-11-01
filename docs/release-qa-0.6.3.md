# Arisa32 0.6.3 release verification

Builds and regression checks ran on 2026-11-01; final package verification completed on 2026-11-02. This release packages the DC2DM compatibility fixes, DC4/DC4PH confirmation settings, image filtering, and floating navigation changes. Android versionName is `0.6.3` and versionCode is `10`.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.3.apk | 13,253,339 | `8c353f379b2b57eeb0bc48539a2cacb03d2829aa7e8ce91a257985de7e262c39` |
| Arisa32-v0.6.3-debug.apk | 14,186,516 | `008dd04401becad49ff1038a9c2556f3b026b079923d3fbcc85bb390e478fa4f` |

Both files match their Gradle outputs and pass ZIP integrity checks. Manifest inspection confirms package `org.dc4engine`, minSdk `21`, targetSdk `36`, and the version above. Only the debug APK is debuggable.

Both packages pass APK v1 and v2 signature verification. Their signing certificate matches the verified published 0.6.2 package: SHA256 `6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Each APK includes `libarisa32`, the C++ runtime, and five FFmpeg libraries for both arm64-v8a and armeabi-v7a. DEX inspection finds `FloatingNavigation`, `FrameFilter`, and `ReleaseNotes`, with no instrumentation test classes. The package check also rejects bundled MES, CRX, CRM, and Arisa32 save files.

Local evidence is recorded in `build/release-0.6.3/packages.json`, including source and library hashes, and in `release-signature.log`, `debug-signature.log`, `previous-signature.log`, `release-badging.log`, and `debug-badging.log` in the same directory. Distribution files and `SHA256SUMS.txt` are under `dist-v0.6.3/`. These generated files are not committed to the source tree.

## Build and regression results

| Check | Result | Local evidence under build/release-0.6.3/ |
| --- | --- | --- |
| Desktop Release build check | PASS: Ninja reported no work to do; this was an incremental build check | `desktop-build.log` |
| Desktop CTest | PASS: 22/22 suites, 0 failures, 45.92 seconds | `desktop-test.log` |
| DC4 confirmations with installed game data | PASS: switches, shared checkbox policy, cancel, persistence, and reset | `dc4-confirmations.log` |
| DC4PH confirmations with installed game data | PASS: switches, shared checkbox policy, cancel, persistence, and reset | `dc4ph-confirmations.log` |
| Android profile checks | PASS: 317 checks | `java-profile-test.log` |
| Android export checks | PASS: 253 checks | `java-export-test.log` |
| Release-note parser and language documents | PASS: 107 checks | `release-notes-test.log` |
| Android release and debug build | PASS: both variants and both ABIs, 6 minutes 47 seconds | `android-build.log` |

The desktop CTest run supplied installed DC2DM and DC4PH game data through `DC2DM_DATA` and `DC4PH_DATA`, plus MS Gothic through `DC2DM_MENU_FONT`. The separate confirmation runs supplied DC4 and DC4PH data directly. Optional installed-data checks for other game profiles remain outside this run's scope.

The Android build used the repository's tracked AGP `8.5.2`. An unrelated local edit to AGP `8.13.2` was backed up, temporarily replaced for the build, and restored afterward; it is excluded from this release commit. Build warnings about compileSdk support, the manifest package attribute, deprecated APIs, and Gradle deprecations did not fail the build.

## Earlier feature validation

These device results validate the feature changes before final versioning and packaging. They are separate runs, not a combined test of the final 0.6.3 APK.

| Feature | Result and scope | Local evidence |
| --- | --- | --- |
| Floating controls and hidden handle | PASS: 431 assertions on I2219/API 36 across DC4, DC4PH, DC2PC, and DC2DM control configurations. Real Activity controls in synthetic game areas cover varied sizes, safe corners, vertical dragging, gesture axis locking, restoration, controller recreation, keyboard use, and 200% text in ID/EN/JP. | `build/navigation-position-validation/navigation-device.log` |
| Image filtering and FPS row | PASS: 187 checks on I2219/API 36. Offscreen GPU output, software fallback, real game sample frames, mode switching, saved settings, one FPS row, localized dialog layout, keyboard actions, and Activity recreation. | `build/texture-smoothing-validation/` |
| DC2DM sidebar | PASS: 89 checks for ordinary-story navigation and replay restrictions; this run predates the final native FACE and CONFIG changes. | [DC2DM validation report](dc2dm-fixes-validation.md) |

The navigation run checks controller recreation, not a full process restart or complete native playthrough. GPU pixel differences establish that filtering runs; they are not a quality score or proof that every frame looks better. Further evidence and captures are described in [DC2DM fixes](dc2dm-fixes-validation.md), [confirmations and navigation](dc4-confirmations-navigation.md), and [image smoothing](texture-smoothing-validation.md).

## Scope and limits

DC2DM remains a compatibility beta. Coverage is limited to the documented scenes, configurations, and fixtures. Every route, device, translation patch, and installed Activity state has not been tested. Android font rasterization may still differ from the PC version. Final 0.6.3 package inspection does not constitute a new device run of its release native libraries or all Activity controls; runtime testing on armv7 is also outside this release check.

Native PC DC2DM save import remains unsupported. Older saves cannot restore portraits they never recorded; new saves retain portrait state. Desktop video remains MPEG-1 only, while MP4/H.264 patches require Android FFmpeg. FXAA requires Android 13 or later and falls back to Bilinear when the GPU path is unavailable. It filters rendered frames without modifying texture assets. FPS and battery impact have not been measured.

## Release copy delivery gate

Antislop mode is `during`, selected earlier in this session. The primary agent checked the release text against the requested fixes, existing repository release style, recorded feature evidence, and package results. Complete Indonesian and English notes carry the same scope, download names, beta status, and limitations. The language requirement is recorded in [the release documentation guide](releases/README.md) for future releases.

This gate covers release documentation and versioning. It introduces no new interface or visual assets. Visual layout, contrast, focus, gestures, and device interaction evidence remain scoped to the earlier feature reports linked above.

| Item | Status and evidence |
| --- | --- |
| R-02 / R-15 / R-16 | PASS: direct factual wording, no em dash in the new release notes, and explicit APK and checksum labels. |
| R-17 / R-36 / R-38 | PASS: versions, hashes, byte sizes, and test counts come from recorded outputs; beta status and test limits are explicit. No measured performance or universal compatibility claim is made. |
| R-18 / R-23 | PASS: no invented people, testimonials, or new visual assets. |
| R-24 | PASS for document scope: language selectors and report links name included files, and downloads name the requested v0.6.3 release. Public links are checked during publication. |
| R-35 | PASS for release scope: Android builds, desktop build check and regressions, and package inspection completed. Earlier device evidence is distinguished from final package checks. |
| R-37 | PASS for copy scope: the user's requested fixes and two-language requirement, with the established release format, supply the direction. |
| C-1 / C-3 / C-5 | PASS: sections explain actual changes, installation choices, validation, and limits; neither language adds unsupported claims. |

Visual purpose and liveliness checks do not introduce new design requirements for these Markdown and version edits. Their applicable interface evidence is recorded in the feature delivery gates.

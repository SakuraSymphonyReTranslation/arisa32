# Arisa32 0.6.6 release verification

Verification completed on 2026-11-03. This release packages larger DC4/DC4PH
dialogue and character names and a translucent floating control handle.
Android versionName is `0.6.6` and versionCode is `13`.

## Packages and build

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.6.apk | 13,286,991 | `56a7276d7d38c6045ba6bec8f6c4eab29c6088bc58b46d1d6ec8023d9c2c6fa1` |
| Arisa32-v0.6.6-debug.apk | 14,222,460 | `168eb44325259c7d651a393b085036ec007a1694aeecb9049939eeaa2f55c0c5` |

Both APKs match the Gradle outputs and pass ZIP integrity and v1/v2 signature
verification. Their manifests report `org.dc4engine`, version 0.6.6/code 13,
minSdk 21 and targetSdk 36. Only the debug APK is debuggable. Both use the same
certificate as the published 0.6.5 APK: SHA256
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Package checks confirm both ABIs, the engine/C++/FFmpeg libraries, and the
existing updater and navigation classes. Neither package contains the test
instrumentation classes or bundled MES, CRX, CRM or Arisa32 save files.
APKs and `SHA256SUMS.txt` are in `dist-v0.6.6/`. Package and source hashes are
recorded in `build/release-0.6.6/packages.json`; signature and manifest logs
are in the same directory. Generated packages and evidence are not committed.

The release/debug build passed in 1 minute 30 seconds using Gradle 9.2.0,
Android Studio's JDK, NDK 27.3.13750724 and tracked AGP 8.5.2. A local init
script selects the tracked AGP version while preserving the unrelated local
AGP 8.13.2 edit outside this commit. Existing SDK, manifest-package and
deprecation warnings did not fail the build. Evidence:
`build/release-0.6.6/android-build.log`.

The final release engine libraries are byte-identical to the feature-tested
APK from before the version bump. Their SHA256 values are:

- arm64-v8a: `8ee315e018468cdc13acbb68bae72a521e6d13765818142ecdac1567f01f46d4`
- armeabi-v7a: `b605052ca1c4c98999e1051462319e8afcaef90da547cfa852da58abc4f16e5c`

## Regression results

Production runtime code did not change between the feature checks and release
packaging. The release step changes version metadata and documentation.

| Check | Result | Local evidence |
| --- | --- | --- |
| Text rendering and scene regressions | PASS: both test executables, including DC4/DC4PH name bounds, three-line dialogue, wrapping, progressive reveal and save restoration | `build/dc4-textbox/check-nameplate.log` |
| Original game renders | PASS: inspected Japanese DC4 and Indonesian DC4PH dialogue and nameplates | `build/dc4-textbox/nameplate-dc4_sen20181222b.mes.png`, `build/dc4-textbox/nameplate-dc4_asa20190429c1.mes.png` |
| Android floating navigation | PASS: 431 checks across DC4, DC4PH, DC2PC and DC2DM | `build/dc4-textbox/navigation-device-confirm.log` |
| Handle appearance on device | PASS: idle cyan composites at 40% opacity; keyboard focus uses full cyan | `build/dc4-textbox/navigation-checks/dc4-hidden.png`, `build/dc4-textbox/navigation-checks/dc4-keyboard.png` |
| Release-note parser and translations | PASS: 131 checks, including both 0.6.6 documents | `build/release-0.6.6/release-notes-test.log` |

Navigation checks ran on an I2219 with Android 16 in the isolated
`org.dc4engine.updateqa` app. They cover taps, horizontal swipes, vertical
dragging and position persistence, cancellation and multitouch, keyboard
focus and Enter, auto-hide, settings, layout/insets and input isolation from
the game. Earlier attempts encountered a locked device and an auto-hide timing
failure; the final unchanged suite passed all 431 checks after unlocking.
The temporary QA app and instrumentation package were removed afterward.
The main app and user saves were not modified by these checks.

The idle handle capture contains RGB `(44,108,144)` over background
`(36,54,75)`, matching the 40% blend of cyan `(56,189,248)`. The keyboard
capture contains full cyan. The 48 x 64 dp touch target remains unchanged.

## Scope and limitations

The DC4/DC4PH default Latin dialogue size changes from 20 to 22 pixels on the
game canvas; character names change from 18 to 22. The Android dialogue window
uses a separate MS Gothic font while menus retain their existing font.
The handle changes apply to profiles using floating navigation. Route
progression and map behavior from 0.6.5 remain available.

Visual dialogue checks use desktop rendering with original game assets and
the portable Android font renderer. Navigation checks run on the Android
device. These are not complete route playthroughs or checks of every patch
and device. Font rasterization can still differ from PC. Native armeabi-v7a
execution and physical older Android devices were not tested. DC2DM remains
in beta.

## Release copy delivery gate

The requested changes and established bilingual release format supply the
direction. Release packaging adds Markdown documentation and version metadata.

| Item | Status and evidence |
| --- | --- |
| R-02 / R-15 / R-16 | PASS: direct descriptions and explicit download labels; no em/en dashes or marketing filler in the new release notes. |
| R-17 / R-36 / R-38 / C-5 | PASS: sizes, opacity and test counts match source and recorded results; test limitations are stated. |
| R-18 / R-23 | PASS: no invented people, testimonials or visual assets. |
| R-24 | PASS for document scope: both language selectors target the supplied translations; report and download links use v0.6.6. |
| R-35 | PASS for release scope: release/debug builds and package verification passed; text, scene and device navigation results are recorded above. |
| R-37 / C-1 / C-3 | PASS: both translations cover the same changes, test scope, limitations, upgrade steps and download artifacts in the existing release format. |

Visual purpose and layout checks add no requirements to these Markdown and
version edits. The handle's reduced idle opacity follows the user's request
to reduce distraction; full opacity on touch or keyboard focus preserves its
active indication. This report does not claim application-wide contrast or
accessibility conformance.

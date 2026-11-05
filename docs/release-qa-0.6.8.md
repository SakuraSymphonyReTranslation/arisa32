# Arisa32 0.6.8 release verification

Verified on 2026-11-05. Android versionName is `0.6.8` and versionCode is `15`.

## Packages and build

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.8.apk | 13,311,414 | `b04ffcbf98305f301abc24bb4dd8b67f6b6417d3129b62116fa2e8875f34f20e` |
| Arisa32-v0.6.8-debug.apk | 14,250,723 | `e8e8091dc066fc9f9e05cba782bc5feb96c12e3edd0811193f5badcc385b1bfe` |

Both APKs match their final Gradle outputs and pass ZIP integrity and v1/v2
signature verification. Their manifests report `org.dc4engine`, version
0.6.8/code 15, minSdk 21 and targetSdk 36. Only the debug APK is debuggable.
Both use the same certificate as the published 0.6.7 APK:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Package checks confirm arm64-v8a and armeabi-v7a, the engine/C++/FFmpeg
libraries, graphics classes, and the existing updater. Neither APK contains
QA instrumentation classes, game scripts, artwork archives, or Arisa32 save
files. `dist-v0.6.8/` contains the two APKs and `SHA256SUMS.txt`. Local evidence
is under `build/release-0.6.8/`; generated packages and device data are not
committed.

The release/debug build passed in 1 minute 33 seconds using Gradle 9.2.0,
Android Studio's JDK, NDK 27.3.13750724, and tracked AGP 8.5.2. The existing
local init script selects that AGP version while preserving an unrelated
local 8.13.2 edit outside the release commit. Existing SDK, manifest, and
deprecation warnings did not fail the build.

## Installation and feature evidence

The production APK updated the existing installation on iQOO Z9x successfully.
`am start -W` reported `Status: ok`; the installed package reports version
0.6.8/code 15, and its SHA256 matches the release artifact above. Logs are
`install.log`, `launch.log`, `installed-version.log`, and `installed-hash.log`.

The final release contains identical native engine libraries for both ABIs
to the tested fullscreen/COMU/gear feature APK. Release preparation changed
the version metadata and release documentation; it added no runtime fixes.
See `native-identity.log` and `packages.json` for the binary comparison and
source hashes.

| Check | Result |
| --- | --- |
| Release notes parser | PASS: 147 checks, including both new translations and their final lines; `release-notes-test.log`. |
| Screen preferences | PASS: 20 host checks of defaults, persistence, independence, and malformed values. |
| Screen behavior | PASS: 440 device checks of fullscreen, Fit/Stretch, touch mapping, floating controls, movie transforms, lifecycle, and keyboard interaction on Android 16/API 36. |
| COMU regression | PASS: both MES versions, preload versus commit, outgoing snapshot, preserve flags, cleared history, save restoration, and existing patch/font behavior. |
| Effect regression | PASS: gear phases, durations, waits, skip, save restoration, existing effects, both games' original atlases, and authored replay triggers. |
| Scene regression | PASS: Scene regressions passed. |

The [feature validation report](fullscreen-comu-gear-validation.md) identifies
the logs, captures, native addresses, script offsets, and recording frames.
Its feature APK retains version 0.6.7 and has a different package hash from
the final 0.6.8 artifacts above. Fullscreen dialogs were checked in EN/ID/JP
at 200% text size before the release build.

## Scope and limitations

Direct device execution used Android 16/API 36. Builds include Android 5+
and both ARM ABIs, but these checks do not establish execution on every
older Android release, driver, or native armeabi-v7a device. The checks are
not a full route playthrough or coverage of every patch. Gear timing follows
the checked PC engine commands because the recording contains repeated
frames and capture jumps. DC2 Dearest Marriage support remains beta.

All previous 0.6.7 fixes remain included; their evidence and limitations are
in the [0.6.7 report](release-qa-0.6.7.md).

## Release copy delivery gate

The user's requested changes and the established bilingual release format
provide the direction. The feature report covers the application UI and
renderer; this gate covers release metadata and Markdown copy.

| Item | Status and evidence |
| --- | --- |
| Hard Gate: R-02, R-17, R-18, R-23, R-24 | PASS: release copy uses verified counts, explicit artifact names, and matching language/report/download links, with no em/en dashes, invented people, or generated assets. |
| Purpose-Gate | PASS: existing headings and feature lists organize the changes and downloads; release packaging adds no visual decoration or controls. |
| Liveliness and R-37 | PASS for document scope: both translations retain the repository's factual release style and identify the APK variants. |
| C-1 through C-5, R-15, R-16, R-35, R-36, R-38 | PASS: translations cover equivalent behavior, installation, and test limits; parser, build, signature, package, and installation evidence appears above. |
| Remaining visual consistency locks | PASS for document scope: packaging preserves the application UI; the feature report records its UI delivery gate. |

Copy audit: the notes name the specific behavior, source versions, artifacts,
and test limits without marketing filler or unsupported claims. The counts
come from the recorded checks; both translations contain no em/en dashes.

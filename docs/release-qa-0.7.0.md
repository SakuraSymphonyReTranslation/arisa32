# Arisa32 0.7.0 release verification

Release preparation date: 2026-11-08, Asia/Bangkok. Android versionName is
`0.7.0` and versionCode is `17`.

## Packages and build

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.7.0.apk | 13,457,576 | `742e9a766548f8ffbb23cdfb8cbc03d14000751b0c58a31b424cbf27d6c44e35` |
| Arisa32-v0.7.0-debug.apk | 14,404,234 | `77cd31028720582c28641eb72cec337d6dc6ee7ef05c9de6f14622b18515b668` |

Both APKs match the final Gradle outputs, pass ZIP integrity, v1/v2 signature
verification, and `zipalign -c -P 16 -v 4`. Their manifests report
`org.dc4engine`, version `0.7.0` / code `17`, minSdk 21 and targetSdk 36.
Only the debug APK is debuggable. Both use the existing certificate:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

The packages include arm64-v8a and armeabi-v7a, the engine/C++/FFmpeg
libraries, the DC2DM scenario-title table, and the existing updater. Both
engine libraries match their respective final stripped build outputs byte
for byte. Package checks exclude QA instrumentation classes, game scripts,
CRX/CRM artwork, and save files. `dist-v0.7.0/` contains both APKs and
`SHA256SUMS.txt`.

The debug APK has the same entries and asset/library content as the final
0.6.9 development APK. Changes are limited to version-bearing DEX/manifest
content and signature records. Its ARM64 engine hash remains
`4afd52936aa2ea3eec805e7c94aaa54ce6a8f3fcef725c4568566ef98647530d`.
The freshly compiled release ARM64 engine hash is
`1a67d704f077c04d1339387e45bf6e09fdaa6eb8891443124bd23b40e9fda705`.

The release/debug build completed with Android Studio's JDK, Gradle
9.0-milestone-1, NDK 27.3.13750724, and AGP 8.5.2. Native compilation used
two jobs per ABI. The local baseline init script selects the tracked AGP
version while preserving the unrelated local AGP 8.13.2 edit outside this
commit. Existing SDK XML, manifest, and Gradle deprecation warnings did not
fail compilation.

The package verifier initially expected aapt's `sdkVersion` field; aapt2
36.1.0 reports `minSdkVersion`. Updating the verifier to accept either field
and Windows line endings resolved that check. The APKs did not change during
this correction. The verified minimum SDK remains 21.

Local build/package evidence is in `build/release-0.7.0/`: `build.log`,
`packages.json`, `package-checks.log`, the release/debug badging, signature,
and alignment logs, and `debug-previous-comparison.json`. Reproduction uses
`build-release.ps1` and `verify-packages.py` in that directory.

## Source and documentation checks

- The final core completed 32/32 desktop tests before version-only release
  preparation: the remaining 30-test CTest suite, the transition test, and
  the relinked menu test. Logs are under
  `build/dc2dm-scene-transition-20261108/` in `full-desktop-tests.log`,
  `focused-tests.log`, and `final-menu-test.log`.
- The same preparation completed 328 Java profile checks. The release
  changes Android version metadata and documentation, with no further
  engine behavior edits. Recorded hashes of the five final transition/menu
  source files match that tested baseline.
- The release-notes parser passed 163 checks, including both new language
  documents and their final lines. Evidence:
  `build/release-0.7.0/release-notes-test.log`.
- Source, fixture, and staged-path review excludes generated
  `save-screen-test-data/`, local APKs/captures, private saves, and the local
  top-level AGP edit. The Android probe's Java source is included as a test
  fixture, outside the application source sets.

## Final release device verification

The production ARM64 JNI library from `Arisa32-v0.7.0.apk` passed all 19
probe phases on the connected I2219 device. The process exited with code 0;
no phase timed out. Each run used isolated save fixtures and read-only links
to the installed game assets.

| Probe phases | Result |
| --- | --- |
| `transitions` | PASS: recorded classroom cover/white/reveal, feathered left-to-right geometry, script barriers, pause, rejection of transient saves, forced Title, and reload. |
| `stage0`, `stage0-restart` | PASS: initial Start confirmation and cancellation before Graduation Days completion. |
| `stage1`, `stage1-restart` | PASS: direct two-story selection and return, with progress retained after restart. |
| `stage2`, `stage2-restart` | PASS: direct three-story selection and return, with progress retained after restart. |
| `DC2DM01.dat`, `DC2DM02.dat`, `DC2DM03.dat`, `DC2DM04.dat`, `DC2DM06.dat` | PASS: native/companion loading, export, restored artwork, and continuation. |
| `rows` | PASS: scenario titles and quick/auto/latest/regular row actions. |
| `menus` | PASS: menu pages, Otome's three voice styles plus Nothing, 1080p CG rendering and next/back, title particles, and pause/resume. |
| `choices` | PASS: native choice restoration, normal/hover/pressed/cancel/mismatched/outside touch states, both recorded branches, and voiced/unvoiced replay. |
| `minigames-suit` | PASS: all three hands, native menu/script entry, touch states, authored Back/Continue/Exit, and isolated runtime save fixture. |
| `minigames-reversi-0`, `minigames-reversi-1`, `minigames-reversi-2` | PASS: both turn orders at each difficulty, CPU response, quit/continue, native menu/script entry, touch states, and isolated runtime save fixture. |

The log records the exact release APK and engine hashes listed above.
Evidence is in `build/dc2dm-save-validation/0c7ab05e503a/result.log` and its
`captures/` directory. The release summary is
`build/release-0.7.0/device-results.json`; the invocation is recorded in
`probe-release.ps1` and `device-probe.log` in the same release directory.

Root visually inspected representative captures of initial confirmation,
two/three-story selection, Load rows, pressed story choices, Reversi CPU
response, return to Title and reloaded minigame choice, and the classroom
white/reveal sequence. Contact sheets are `release-visual-check.png` and
`release-transition-check.png` in the local release evidence directory.
These inspections supplement the assertions; they do not establish exact
PC parity for every rendered frame.

## Installation and save integrity

`adb install -r` successfully updated the existing installation with the
release APK. `am start -W` returned `Status: ok`; package metadata reports
`0.7.0` / code `17`. Hashing the installed `base.apk` produced
`742e9a766548f8ffbb23cdfb8cbc03d14000751b0c58a31b424cbf27d6c44e35`,
matching the release artifact.

Recursive SHA256 inventories taken before the release probes and after
installation and launch contain the same 16 phone save files and 10 PC save
files. Neither inventory has added, missing, or changed files. The update
did not uninstall the app, clear its data, or restore player save files.

Evidence: `install.log`, `launch.log`, `installed-package.log`,
`installation.json`, `save-integrity.json`, `phone-saves-before.txt`,
`phone-saves-after.txt`, `pc-saves-before.json`, and `pc-saves-after.json`
under `build/release-0.7.0/`.

## Feature evidence and limits

These reports retain the artifact versions, hashes, and test counts from
their original development runs. Their tests are not relabeled as new runs
of the versioned release APK:

- [PC saves and progress](dc2dm-save-progress-validation.md): native and
  companion restoration, imported catalogue progress, numbering, dates,
  and translated scenario titles.
- [Menu audio, CG, and title particles](dc2dm-menu-audio-transition-validation.md):
  Otome voice banks, full-source previews, category navigation, title
  circles, and story order.
- [Choices and voice replay](dc2dm-choice-voice-validation.md): original
  choice states, both recorded branches, and voiced/unvoiced replay.
- [Minigames](dc2dm-minigame-validation.md): suit/Reversi controllers,
  script results, dynamic counts, and discarded results after forced Title.
- [Start and scene transitions](dc2dm-start-scene-transition-validation.md):
  two/three-story selection without another prompt, authored masks,
  native timing/cropping, and transition/input-wait cleanup.

DC2DM remains beta. The checks do not establish a complete playthrough of
every route, acoustic parity for every voice clip, every translation patch,
or runtime compatibility on every Android version or a physical ARMv7
device. Global progress export to PC `system.dat` and direct PC loading of
exported slots are not verified. Saving does not refresh `DC2DM03.dat`.

The Title-to-story-selector boundary still passes through black instead of
the PC crossfade. Native tile particles, flash/shake, some sprite effects,
and complex pending/committed scene sequences remain incomplete. Reversi
does not reproduce the executable's complete random sequence. These limits
appear in both language notes and in the GitHub release body.

## Release copy delivery gate

Root reviewed the release copy against the updated changelog, feature
reports, package checks, and final device results. This gate covers release
packaging and documentation. The linked feature reports retain the UI
checks and limitations from implementation.

The requested bilingual release and the existing release-document format
supply the direction: state the DC2DM changes, their limits, and the files
to download. The document uses ENERGY 1, RHYTHM 1, MOTION 1: plain Markdown,
feature groups separated by whitespace, a version heading as the focal
point, and language/download links as the functional accent. Application
visuals retain the user's PC references.

| Item | Status and evidence |
| --- | --- |
| Hard Gate: R-02 | PASS: the new language notes, GitHub body, and this report contain no em/en dashes. |
| Hard Gate: R-17, R-18, R-36, R-38 | PASS: version numbers, 279 titles, package hashes, and test counts have source evidence; no testimonials, generated identities, or unsupported full-parity claims appear. |
| Hard Gate: R-23, R-33 | PASS for packaging scope: no new visual assets, navigation structure, or application UI features were introduced during release preparation. |
| Hard Gate: R-24, R-26 | PASS: language and report links resolve to the staged documents; download labels name the verified APKs and checksum file. The release targets tag v0.7.0. |
| Hard Gate: R-03, R-25, R-27, R-28, R-32, R-34 | PASS for document scope: standard Markdown links and headings retain the host renderer's layout, focus, and theme behavior. No custom widgets, data-loading views, or FAQ were added. This is not a new accessibility certification of the game UI. |
| Hard Gate: R-35 | PASS: both packages built and passed integrity checks; the production release library passed the 19 recorded probe phases above; the installed release launched successfully. The parser passed 163 checks. |
| Hard Gate: R-37 | PASS: the user's bilingual request and the existing release structure define the copy direction; feature presentation follows the supplied PC references. |
| Purpose-Gate | PASS for document scope: headings separate feature behavior, limits, evidence, and downloads. No decorative icons, backgrounds, gradients, illustrations, animations, or card layouts were added during packaging. |
| Liveliness | PASS for document scope: explicit static-document dials, version focal point, structural whitespace, functional links, and the repeated Indonesian/English feature structure match the stated direction. |
| C-1, C-3, R-05, R-20, R-31 | PASS: each section serves release changes, verification, known limits, or installation; the DC2DM-specific content determines the structure. |
| C-2, R-15 | PASS: links identify their language, report, or download destination; artifact filenames identify the installation packages. |
| C-4, R-11, R-21, R-29, R-30 | PASS for packaging scope: documentation adds no custom radius, theme, palette, or application layout. Existing feature UI checks and limits remain linked above. |
| C-5, R-16 | PASS: claims are tied to scripts, artifacts, and recorded checks; the copy contains no marketing buzzwords or fabricated statistics. |

Copy audit: both translations state the same behavior, installation steps,
files, and limits. The GitHub body includes complete Indonesian and English
sections. Specific game labels and observed behavior replace generic
promotional phrasing. The linked language documents remain available to
the in-app updater on `main`.

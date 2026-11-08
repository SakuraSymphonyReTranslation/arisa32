# Arisa32 0.7.1 release verification

Verified on 2026-11-08, Asia/Bangkok. Android versionName is `0.7.1`,
versionCode is `18`, and the package remains `org.dc4engine`.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.7.1.apk | 16,440,457 | `9d5fe8a369560ed552486ecbc7cb76621410b088c5eb92656d7df4ca65b58251` |
| Arisa32-v0.7.1-debug.apk | 17,427,868 | `b9ab181ce72ad2c4c89e2fa9ebb3347839a8b0cb4d03aac8456471a7ccd823ec` |

Both APKs pass ZIP integrity, v1/v2 signature verification, and
`zipalign -c -P 16 -v 4`. They use the existing signing certificate:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
Only the debug APK is debuggable. All target API 36.

Both packages include ARM64 and ARMv7, require API 21, and extract compressed
native libraries. The public release contains these two APKs and their
checksum file. The release APK was renamed from its original universal
asset name; its bytes and signature are unchanged. The separate ABI assets
were withdrawn. This correction does not rebuild the APKs or move the
published v0.7.1 tag.

Every ARM64 ELF load segment has at least 16 KB alignment. The ARMv7 engine
and FFmpeg libraries also have 16 KB alignment; the ARMv7 `libc++_shared.so`
uses the NDK's 4 KB alignment. It matches NDK 27.3.13750724's library after
`llvm-strip --strip-unneeded`, with SHA256
`a95c07c255e481878b853385942c9179313123619652da3269d8edbd71158588`.
The 16 KB memory-page compatibility claim applies to ARM64.

The packaged fonts and license notices match their source bytes. There is
one Pretendard asset and no `assets/fonts/default.ttf`. APK checks also
confirm the scenario-title table and updater classes, including the new
ABI selector, and exclude QA classes, game scripts, CRX/CRM artwork, and saves.

Builds used Android Studio's JDK, Gradle 9.0-milestone-1, AGP 8.5.2,
NDK 27.3.13750724, and two native compiler jobs. A local Gradle init script
selected the tracked AGP version; the unrelated local AGP 8.13.2 edit is
excluded from this release. SDK XML and Gradle deprecation warnings did
not fail the builds. Local build and package evidence is under
`build/release-0.7.1/`, including `packages.json`, signature/alignment logs,
and `package-checks.log`. The two-package correction was rechecked under
`build/release-0.7.1-universal-correction/`; its `packages.json` records the
canonical filenames. Distribution checksums are in `SHA256SUMS.txt`.

## Source and updater verification

- The desktop Release build completed and all 34 CTest tests passed.
  The 116 recorded native/build inputs remained unchanged through testing
  and the final source check.
- Java profile checks passed 328 assertions. The release-note parser passed
  171 checks against the bilingual release documents.
- The earlier ABI selector suite passed 120 JVM checks, covering ABI
  preference, release/debug preference, unsupported ABIs, universal
  fallback, and the API 22/23 boundary. These checks included the former
  four-asset release layout.
- The corrected suite passed 143 JVM checks. It uses the canonical release
  name and checks both release/debug orders for ARM64, ARMv7, and unknown
  ABIs on API 21, 22, 23, and 36, while retaining internal ABI-selection
  coverage. The bilingual release-note parser still passes all 171 checks.
- Source and fixture review excludes the unrelated top-level AGP edit,
  generated outputs, and local player saves. The added test fixtures create
  synthetic data or read original assets from caller-supplied locations.
- Staged whitespace checks pass for authored files. The copied upstream
  `Pretendard-OFL.txt` retains its original trailing space and final blank
  line; its packaged bytes are preserved.

The public release now has only one non-debug APK. Both the current updater
and older updaters select `Arisa32-v0.7.1.apk`, including when GitHub lists
the debug APK first. The current helper ranks the universal release above
debug; the earlier updater skips debug assets. No runtime change is needed
for the corrected asset names.

## Earlier ARM64 feature checks

The earlier ARM64-only production build was tested on
an iQOO Z9x / I2219 running Android API 36. Its ARM64 engine SHA256 is
`d6891c4c49dc0ddd54413a311c455b5ed0ea79aa6f0ae088399aabeffb2f0af9`.
Device probes use isolated save fixtures and read-only links to game assets.

| Check | Result |
| --- | --- |
| DC2DM read history | All three existing/new/profile-only cases retain their read keys across initialization and shutdown. |
| DC2DM minigame entry | Scripted entry, suit and Reversi transitions, and return/reload paths pass. |
| DC2DM title progression | All three progress stages and their restart cases pass. |
| DC2DM story selection | All three stories pass transition checks, including dialogue entry or opening interruption. |
| COMU | Both DC4/DC4PH corpus runs report zero failures. All 426 captured phone regions match the previously validated Android baseline, with zero differing pixels. |
| DC2PC gallery | 56 original-asset checks and 80 synthetic checks pass, with zero reported differences. They cover navigation, fit/stretch rendering, panning, and dynamic display sizes. |

DC2DM passed all 13 selected phases. Root visually reviewed six final DC2DM
captures showing the story selector, its fade, dialogue, interrupted opening,
minigame choice, and Reversi entry. Four final COMU samples retain their
text within the tested phone regions. Local contact sheets are
`dc2dm-visual-check.png` and `comu-visual-check.png` in the release evidence
directory. The COMU comparison uses a recorded Android baseline; it is not
a new complete PC/Android comparison.

The feature reports retain their original development-build evidence:
[DC2DM menus and fonts](dc2dm-menu-font-validation.md),
[Pretendard layout](dc2dm-pretendard-layout-validation.md),
[COMU padding](comu-pretendard-padding-validation.md), and
[DC2PC gallery](dc2pc-cgmode-validation.md).
These targeted release checks do not repeat a complete playthrough.
Fully playable status comes from the user's completed-game testing.
The universal release's ARM64 engine SHA256 is
`9a4fd64278c5a536e58cd17c0d1299722401d087b8138ad1293ddb2a119cc538`.
Its engine binary differs from the earlier ARM64-only build; its six other
ARM64 native libraries match byte for byte. The feature probes above were
not rerun on the universal package during this filename correction.
ARMv7 and API 21/22 have not been exercised on physical devices for this
release. ELF/ZIP checks verify ARM64 alignment; the connected device uses
4 KB memory pages.

## Installation, storage, and saves

`adb install -r` successfully installed the canonical universal release
over the existing app. A fresh pre-install hash showed that the phone
already had the same universal APK bytes. The installed `base.apk` hash
matches the release asset above, and `am start -W` reports `Status: ok`.
The library screen displays v0.7.1. Android's native-loader log confirms
successful loading from the installation's extracted
`lib/arm64/libarisa32.so`.

Android StorageStats reported the following on the same phone:

| Measurement | Before: 0.7.1 universal | After reinstall and launch: 0.7.1 universal |
| --- | ---: | ---: |
| App bytes | 26,207,744 | 26,207,744 |
| Data bytes, including cache | 9,015,296 | 9,158,656 |
| Cache bytes, already included above | 0 | 143,360 |
| App + data bytes | 35,223,040 | 35,366,400 |

The measured total is **35.37 MB**, using decimal MB. It describes the
current app data after opening the library, rather than a complete gameplay
and font-selection session. Private font files were not readable through
`run-as` in this non-debug installation. Both fonts are packaged, and font
selection can extract additional files into app data. Game assets in separate
game folders are outside this app-storage total. Imports, caches, and saves
can change the total.

The earlier **31.54 MB** measurement used the withdrawn ARM64-only package
and a different data snapshot. It is not a measurement of this universal
APK. The original snapshots remain in `build/release-0.7.1/`. This correction
does not clear app data, remove fonts, or change the APK contents.

Fresh recursive hashes before and after the universal reinstall and launch
match for all 160 external save/profile files: DC4 27, DC4PH 49,
Sakuramusubi 49, DC2DM 16, and DC2PC 19. The update did not uninstall the
app or clear its data. Private files/preferences were backed up during the
earlier release validation. They were not re-read for this correction,
because the non-debug package does not expose `run-as` access.

Local evidence includes `before-device.json`, `after-device.json`,
`save-storage-verification.json`, `installation.json`, `production-logcat.log`,
and `production-ready.png` under
`build/release-0.7.1-universal-correction/`.

## Scoped delivery gate

Root reviewed the release copy against the existing Indonesian and English
README structure. The implementation's font/menu direction and detailed
UI gate remain in the linked feature reports. This correction changes
public asset names and documentation without changing runtime code or layout.

- Hard gate PASS: release claims have the package, device, and source
  evidence above; new prose contains no em dash or invented testimonial.
- Purpose gate PASS: the existing language links and APK table distinguish
  release and debug downloads, with both ABIs included in each package.
- Voice and structure PASS: both READMEs retain their technical prose and
  section order, with matching original-data and translation-patch guidance.
- Craftsmanship PASS: both languages agree, internal document targets exist,
  both APKs pass package verification, the universal release installs and
  launches, and the measured storage scope is explicit.

# Arisa32 0.7.1 release verification

Verified on 2026-11-08, Asia/Bangkok. Android versionName is `0.7.1`,
versionCode is `18`, and the package remains `org.dc4engine`.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.7.1.apk | 16,440,457 | `9d5fe8a369560ed552486ecbc7cb76621410b088c5eb92656d7df4ca65b58251` |
| Arisa32-v0.7.1-debug.apk | 17,427,868 | `b9ab181ce72ad2c4c89e2fa9ebb3347839a8b0cb4d03aac8456471a7ccd823ec` |
| Arisa32-v0.7.1-arm64-v8a.apk | 18,029,003 | `1a14d6a4afce0f9eee86698693cf40250d7669199562734282074c6569d66655` |
| Arisa32-v0.7.1-armeabi-v7a.apk | 16,510,555 | `0028ed1f3ba35666a4796e057a851e395204060b66a4383f12e9d29cbf155615` |

All four APKs pass ZIP integrity, v1/v2 signature verification, and
`zipalign -c -P 16 -v 4`. They use the existing signing certificate:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
Only the debug APK is debuggable. All target API 36.

The two universal packages include ARM64 and ARMv7, require API 21, and
extract compressed native libraries. The individual ABI packages require
API 23 and load uncompressed native libraries directly from the APK. Every
native ZIP entry in those packages starts at a 16 KB boundary. This makes
the ARM64 download larger than the universal download, while reducing its
installed storage by avoiding extracted library copies.

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
and `package-checks.log`. Distribution checksums are in `SHA256SUMS.txt`.

## Source and updater verification

- The desktop Release build completed and all 34 CTest tests passed.
  The 116 recorded native/build inputs remained unchanged through testing
  and the final source check.
- Java profile checks passed 328 assertions. The release-note parser passed
  171 checks against the bilingual release documents.
- The ABI selector passed 119 JVM checks, including all 24 permutations of
  the four APK names, release/debug preference, unsupported ABIs, fallback
  to universal packages, and the API 22/23 boundary.
- Source and fixture review excludes the unrelated top-level AGP edit,
  generated outputs, and local player saves. The added test fixtures create
  synthetic data or read original assets from caller-supplied locations.
- Staged whitespace checks pass for authored files. The copied upstream
  `Pretendard-OFL.txt` retains its original trailing space and final blank
  line; its packaged bytes are preserved.

The new updater selects a compatible individual ABI package on API 23+
and the universal package on API 21/22. Older updaters select the first
non-debug APK in the release asset list; the universal release APK is
uploaded first to preserve their compatibility.

## Final ARM64 device checks

The production libraries from `Arisa32-v0.7.1-arm64-v8a.apk` were tested on
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
ARMv7 and API 21/22 have not been exercised on physical devices for this
release. ELF/ZIP checks verify ARM64 alignment; the connected device uses
4 KB memory pages.

## Installation, storage, and saves

`adb install -r` successfully updated the existing installation to the
ARM64 release package. The installed `base.apk` hash matches the asset
above, and `am start -W` reports `Status: ok`. The library screen displays
v0.7.1. Android's native-loader log confirms successful loading of
`base.apk!/lib/arm64-v8a/libarisa32.so`, demonstrating direct loading from
the production APK.

Android StorageStats reported the following on the same phone:

| Measurement | Before: 0.7.0 debug | After: 0.7.1 ARM64 release |
| --- | ---: | ---: |
| App bytes | 26,933,760 | 18,376,192 |
| Data bytes, including cache | 13,152,256 | 13,160,448 |
| Cache bytes, already included above | 290,816 | 303,104 |
| App + data bytes | 40,086,016 | 31,536,640 |

The measured total is **31.54 MB**, using decimal MB, a reduction of
8.55 MB from the captured pre-release installation. The post-install
measurement follows opening the production app; gameplay probes use the
isolated fixtures described above. Both extracted fonts were already in
app data: MS Gothic (8,990,160 bytes) and Pretendard JP Bold (3,834,112 bytes).
The duplicate private `default.ttf` had been removed before both snapshots;
this release does not automatically delete imported fonts. Game assets in
the separate game folders are outside this app-storage total. Further
imports, caches, and saves can change the total.

Recursive hashes before the release probes and after installation/launch
match for all 160 external save/profile files: DC4 27, DC4PH 49,
Sakuramusubi 49, DC2DM 16, and DC2PC 19. The update did not uninstall the
app or clear its data. A private files/preferences archive was captured
before installation. Private hashes were not re-read after installing the
non-debug package, which does not expose `run-as` access.

Local evidence includes `before-device.json`, `after-device.json`,
`save-storage-verification.json`, `installation.json`, `production-logcat.log`,
and the production launch screenshot under `build/release-0.7.1/`.

## Scoped delivery gate

Root reviewed the release copy against the existing Indonesian and English
README structure. The implementation's font/menu direction and detailed
UI gate remain in the linked feature reports. This release preparation adds
packaging, updater selection, and documentation without changing UI layout.

- Hard gate PASS: release claims have the package, device, and source
  evidence above; new prose contains no em dash or invented testimonial.
- Purpose gate PASS: the existing language links and APK table distinguish
  language and Android/ABI requirements; no decorative UI was introduced.
- Voice and structure PASS: both READMEs retain their technical prose and
  section order, with matching original-data and translation-patch guidance.
- Craftsmanship PASS: both languages agree, internal document targets exist,
  the APKs build and launch, and the measured storage scope is explicit.

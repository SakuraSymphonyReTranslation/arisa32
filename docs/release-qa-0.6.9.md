# Arisa32 0.6.9 release verification

Verified on 2026-11-06. Android versionName is `0.6.9` and versionCode is `16`.

## Packages and build

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.9.apk | 13,355,591 | `103a77023d40009157f388fdd0cd0bc926e7c5ee966975cda3cf5d73172211c3` |
| Arisa32-v0.6.9-debug.apk | 14,296,659 | `ed47fca1c10403d41de357ea3b12054623677d013373eae66c47565b9687cab3` |

Both APKs match the final Gradle outputs and pass ZIP integrity and v1/v2
signature verification. Their manifests report `org.dc4engine`, version
0.6.9/code 16, minSdk 21 and targetSdk 36. Only the debug APK is debuggable.
Both use the same certificate as the published 0.6.8 APK:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Package checks confirm arm64-v8a and armeabi-v7a, the engine/C++/FFmpeg
libraries, cursor/graphics classes, and the existing updater. Neither APK
contains QA instrumentation classes, game scripts, artwork archives, or
Arisa32 save files. `dist-v0.6.9/` contains both APKs and `SHA256SUMS.txt`.
Generated packages, test fixtures, and personal game/save data are excluded
from the commit. Local release evidence is under `build/release-0.6.9/`.

The release/debug build passed in 24m 14s using Gradle 9.2.0, Android
Studio's JDK, NDK 27.3.13750724, and tracked AGP 8.5.2. A local init script
selects that AGP version while preserving an unrelated local 8.13.2 edit
outside the release commit. Existing SDK XML, compileSdk compatibility,
manifest, and deprecation warnings did not fail the build.

## Final verification

| Check | Result |
| --- | --- |
| Desktop build and CTest | PASS: 27/27 tests, with DC4_DATA and DC4PH_DATA pointing to the installed games. Covers routes, menus, sprites, portraits, effects, save compatibility, and earlier engine regressions. |
| Release notes parser | PASS: 155 checks, including the new Indonesian and English documents and their final lines. |
| Production arm64 JNI | PASS: replay, map routing, fresh/save-restart progression, and CP932 translation probes using the library from the final release APK. |
| Production arm64 portraits | PASS: the message portrait regression executable linked against the final APK library and ran on Android. |
| Package and signing checks | PASS: package metadata, both ABIs, ZIP integrity, matching certificate, expected classes, excluded QA/game data, and distribution/output byte identity. |
| Installation | PASS: `adb install -r` updated the existing iQOO Z9x installation; `am start -W` reported `Status: ok`. Installed version is 0.6.9/code 16 and the pulled APK matches the release artifact byte for byte. |

Logs: `desktop-tests.log`, `release-notes-test.log`, `replay-probe.log`,
`portrait-probe.log`, `packages.json`, `install.log`, `launch.log`, and
`installation.json`. Release preparation changed version metadata and
documentation; the completed feature implementation is included in this commit.

## Feature evidence

These reports record earlier feature checks and their original APK versions.
Their device counts are historical evidence, not additional runs of every UI
suite against the versioned release APK.

- [Sprite motion](dc4-sprite-motion-validation.md): shared motion queues,
  script timings, all five sprite slots, recording comparisons for Nino/Sorane,
  actual-script replay for both games, and snapshot v11. Its earlier snapshot
  fixture failures were repaired before the final 27/27 CTest run above.
- [D.C. progression](dc4-progression.md): all eight endings, inconsistent
  profile/import/slot cases, confirmation rechecks, and a separately requested
  personal profile correction. The generic release guard does not reconstruct
  genuine progress when an old profile incorrectly marks every ending complete.
- [Dialogue portraits](dc4-message-face-validation.md) and
  [portrait inventory](dc4-portrait-inventory.md): original FACE assets,
  placement, cleanup, and save restoration.
- [CG resolution](cg-preview-hd-validation.md): 810 Android checks, rendered
  pixel/detail comparisons, and device-sized source rendering. See also
  [gallery navigation](cg-preview-back-validation.md) and
  [gallery rendering](cg-preview-render-validation.md).
- [Cursor mode](cursor-mode-validation.md): 209 Android checks and 11 host
  preference checks, including gesture behavior, lifecycle, EN/ID/JP settings,
  keyboard access, and 200% text size.
- [Game-folder checks](dc4ph-startup-validation.md): 64 Android checks of
  incomplete bases, registration, scans, launch errors, and partial overlays.
- [Locked music marker](music-locked-marker-validation.md): all 42 DC4 and
  43 DC4PH music rows while preserving lock behavior.

## Limits

Direct device execution used Android 16/API 36. Builds include Android 5+
and both ARM ABIs; these checks do not establish execution on every older
Android release, driver, or physical armeabi-v7a device. This is not a full
playthrough of every heroine route or verification of every translation patch.

Direct sprite recording comparisons cover Nino and Sorane; shared-script
coverage does not prove visual parity for every character animation. Some
per-pixel sprite masks remain approximate. Full PC parity for gallery timing
and music marker text is not established. Old saves remain readable but lack
newly recorded live-motion state. DC2 Dearest Marriage support remains beta.

The historical broader Android UI harness has a separate sidebar bubble-position
assertion noted in the gallery reports. The feature-specific suites above passed;
this release report does not claim that the entire Android UI harness was rerun.

## Release copy delivery gate

The requested release and the repository's bilingual format supply the copy
direction. Packaging retains the existing document structure and application
visual design; the feature reports above contain their scoped UI gates.

| Item | Status and evidence |
| --- | --- |
| Hard Gate: R-02, R-17, R-18, R-23, R-24, R-36, R-38 | PASS: matching translations use recorded test counts, named artifacts, and checked language/report links. Copy contains no em/en dashes, invented people, generated art, or unsupported parity claims. |
| Purpose-Gate | PASS for document scope: headings separate behavior, limitations, and downloads. No decorative components or visual assets were added during packaging. |
| Liveliness and R-37 | PASS for document scope: established release voice, version heading as focal point, and whitespace separating feature groups. Application design direction is recorded in the feature reports. |
| C-1 through C-5, R-15, R-16, R-35 | PASS: each section describes a shipped behavior or test limit; filenames identify the download action. Parser, builds, package checks, production probes, and installation provide execution evidence. |
| Remaining visual consistency locks | PASS for packaging scope: no application visual changes were made during release preparation; feature UI checks and limits are linked above. |

Copy audit: both translations cover the same behavior, installation steps,
counts, artifacts, and limitations. The claims name their evidence and avoid
marketing filler. GitHub uses the full Indonesian document with an absolute
English selector; the app can retrieve both complete language documents.

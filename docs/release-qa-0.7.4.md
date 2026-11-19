# Arisa32 0.7.4 release verification

Verified on 19 November 2026 (Asia/Bangkok). Both universal APKs use
versionName `0.7.4`, versionCode `22`, and package `org.dc4engine`.
The release includes DC4 progress commit
`c3eb9b625ced963c11d870173e6f7bb3d0b0b061` and the experimental
MangaGamer Dal Segno continuation.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.7.4.apk | 16,562,651 | `3814ff901a6812e96fbeb6d1011ac7797c42339a229c1f81986406fe5368569b` |
| Arisa32-v0.7.4-debug.apk | 17,558,564 | `0036e9b5d7529c69c1b42e3643221f96e911170371d71f5bfc3889d4d66c3a4d` |

Both APKs pass ZIP integrity, v1/v2 signature verification, and
`zipalign -c -P 16 -v 4`. Their signing certificate matches earlier releases:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
Both contain ARM64 and ARMv7, require API 21, target API 36, and extract
compressed native libraries. Only the debug APK is debuggable.

Every ARM64 ELF LOAD segment has at least 16 KB alignment. ARMv7 engine
and FFmpeg libraries also meet 16 KB alignment; the NDK ARMv7
`libc++_shared.so` meets 4 KB alignment. These inspections do not replace
testing on a physical ARM64 device with 16 KB memory pages.

All packaged assets match source bytes, including MS Gothic, Pretendard JP
Bold, license notices, and scenario-title tables. The DC2TY table retains
129 unique scenario IDs. Production updater classes and the new
`DalSegnoProfile` are present in DEX. Tracked QA classes, instrumentation,
game scripts, CRX/CRM artwork, player saves and their companions are absent.
There is no `assets/fonts/default.ttf`.

The combined release/debug build passed in 10m 6s with 87 tasks:
65 executed and 22 up-to-date. It reused standard Gradle outputs with
Android Studio JBR 21, Gradle 9.0-milestone-1, AGP 8.5.2 and NDK
27.3.13750724. The existing Gradle init script selected tracked AGP 8.5.2
while preserving the unrelated local 8.13.2 edit. SDK XML, compileSdk support,
manifest package, deprecated API and Gradle warnings did not fail the build.

## Feature and updater checks

- Previously completed continuation tests passed for `mesopcode_test`,
  `mesvm_test`, `save_host_test`, default `scene_test`,
  `scene_test --dalsegno`, `audio_test`, `dc4_route_test`, and
  `system_menu_test`. DC4/DC4PH progression checks used both installed games.
- Dal Segno scene checks cover six/seven title actions, 11 toolbar targets,
  and 11 slider drags across five CONFIG tabs. Audio regressions cover the
  corrected voice/SE stop sentinels and character-volume mute.
- Android host profile checks passed 493 assertions, including detection
  of Dal Segno in a neutral directory with a large NAMES.DAT, separation from
  Da Capo and DSIF, English base content, and the MangaGamer phone scan path.
  The old six-profile assertion was updated for the seventh registered profile.
- Release-note parsing passed 195 checks across all localized documents,
  including both v0.7.4 files and their final lines.
- Download checks passed 249 assertions across 48 scenarios. Asset selection
  passed 143 checks with the original fixture and another 143 with v0.7.4 names.
- `git diff --check` and the staged equivalent passed.

Feature tests used the development 0.7.3/code21 build before the release
version bump. This release step rebuilt and inspected both final code22 APKs;
it did not repeat the already-passing native feature suites after documentation
and version changes. See [Dal Segno validation](dalsegno-validation.md).

The DC4/DC4PH fix tracks authored ending awards separately from imported
slot flags, migrates legacy profiles individually with a backup, preserves
confirmed global progress, and accepts legacy snapshot fingerprints.
Earlier phone verification checked automatic Plus Harmony migration,
restart persistence, 320 unchanged non-profile files and all 64 retained story
slots. Details and limits are in [DC4 progression](dc4-progression.md) and
[legacy save compatibility](dc4ph-legacy-save-compatibility.md).

The canonical updater asset is `Arisa32-v0.7.4.apk`; the debug suffix
excludes the other APK from normal update selection. `SHA256SUMS.txt`
contains the two distribution hashes.

## Documentation and scope

Both release documents describe the same changes, experimental status and
support limits. Each has one localized H1 and the horizontal
Bahasa Indonesia | English selector. The GitHub release body matches the
Indonesian document except for its absolute English selector URL. Both files
are pushed to main before publication, where the app fetches localized notes.
Both READMEs, the release index and CHANGELOG point to v0.7.4.

Dal Segno is not fully playable. User testing reports missing character
sprites, slow button responses, unavailable EXTRA and other incomplete menu
functions. The title test proves behavior under the authored unlock flag;
it does not verify every route's ending awards or automatic Dal Segno profile
repair. Native Dal Segno save round trips remain unverified.

The final code22 APKs were not installed or exercised on a phone during this
release step. Prior development installations and device observations are
separate evidence. ARMv7 devices, API 21/22, physical 16 KB page devices,
and complete playthroughs of every supported game were not exercised.
The full installed system-menu asset suite was not enabled in the continuation.

Temporary build logs, package inspections, source hashes and host-test output
remain in the task's Windows Temp directory,
`arisa32-release-0.7.4-20261119`. The release excludes unrelated build-tool
edits, cleanup work, generated save fixtures, user saves and temporary outputs.

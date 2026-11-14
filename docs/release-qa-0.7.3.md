# Arisa32 0.7.3 release verification

Verified on 14 November 2026 (Asia/Bangkok). Android versionName is
`0.7.3`, versionCode is `21`, and the package remains `org.dc4engine`.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.7.3.apk | 16,542,089 | `f0c3b27f4d6325c76536c2bbb8bdccfa0ae7ed2188e8c48086f577f18f47501a` |
| Arisa32-v0.7.3-debug.apk | 17,537,076 | `0b84d756ff3097558f571b98c982508dde5971983644e4717c4bd25e718b47a3` |

Both APKs pass ZIP integrity, v1/v2 signature verification, and
`zipalign -c -P 16 -v 4`. Their signing certificate matches earlier releases:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
Each contains ARM64 and ARMv7, requires API 21, targets API 36, and extracts
compressed native libraries. Only the debug APK is debuggable.

All ARM64 ELF load segments have at least 16 KB alignment. ARMv7 engine
and FFmpeg libraries also have 16 KB alignment; the NDK ARMv7
`libc++_shared.so` has 4 KB alignment. ARMv7 does not establish compatibility
with physical ARM64 devices using 16 KB memory pages.

Every packaged asset matches its source bytes, including MS Gothic,
Pretendard JP Bold, license notices, the vgmstream notice, and all scenario
title tables. The DC2TY table contains 129 unique scenario IDs. Checks
confirm the production updater and DC2TY profile classes and exclude 24
distinct test classes, QA instrumentation, game scripts, CRX/CRM artwork,
and save companions. There is no `assets/fonts/default.ttf`.

The combined release/debug build completed in 7m 43s, with 60 executed
tasks and 27 up-to-date tasks. It used Android Studio JBR 21, Gradle
9.0-milestone-1, AGP 8.5.2, and NDK 27.3.13750724. The existing Gradle init
script selected the tracked AGP version while preserving the unrelated
local AGP 8.13.2 edit. SDK XML, compileSdk support, deprecated API, and
Gradle warnings remain in the build log.

## Feature and updater checks

- Six relevant CTest tests passed in 93.06 seconds: `dc2pc_menu_test`,
  `dc2ty_menu_test`, `dc2dm_menu_test`, `system_menu_test`, `text_render_test`,
  and `confirm_dialog_test`.
- Music checks cover all 74 titles with MS Gothic and Pretendard JP, row
  hover/press/cancel, selected-title persistence, and scrollbar dragging.
- Scene checks cover all 10 cards with both fonts, retained date pixels,
  cursor and direct-touch presses, drag cancellation, and confirmation.
- Android native renderer/input probes passed 120 Scene Replay checks
  plus 36 checks using the Japanese base content and actual Indonesian
  patch. Five 1280x720 captures matched the desktop renderer exactly.
- The release-note parser passed 187 checks, including the corrected
  historical notes and both v0.7.3 documents.
- Download checks passed 249 assertions across 48 scenarios. Asset
  selection passed 143 checks with the original fixture and another 143
  with v0.7.3 filenames.

Feature checks used the development 0.7.2/code20 build before the version
bump. Their source changes are included in v0.7.3. Full evidence and
boundaries are recorded in the [Music report](dc2pc-music-mode-fix.md) and
[Scene Replay report](dc2pc-scene-replay-fix.md).

The canonical updater filename is `Arisa32-v0.7.3.apk`. The separate
`-debug.apk` suffix keeps the debug package out of normal update selection.
`SHA256SUMS.txt` contains the hashes of both distribution APKs.

## Documentation

Indonesian and English notes describe the same changes and support limits.
Each has one localized H1 and a horizontal language selector. The GitHub
release body matches the Indonesian notes except for the absolute English
selector URL. Both language files must be on `main` before publication
because the in-app updater fetches localized notes from that branch.

Both READMEs and the v0.7.2 notes now describe the original DC2PC/DC2TY
textbox history controls and the absence of a separate native backlog
screen. DC2DM has its own backlog interface. The DC2TY implementation
report was corrected consistently; this is a documentation correction.

GitHub Markdown rendering confirms that the corrected README titles and
release-note titles do not produce strikethrough. Historical v0.3.0 and
v0.5.5 title corrections add only code-span delimiters. Code spans in
release notes preserve literal tildes in both GitHub and the app parser.

Seven selected evidence PNGs were reviewed as a contact sheet, with the
Scene Replay and Music press details also inspected at larger size.
They show game menus and technical annotations. Intermediate frames,
captures, build logs, and package inspection data stay in Windows Temp.

## Verification scope

This release step builds and inspects the final code21 APKs; it does not
install them on a phone. The earlier development APK was installed with
an in-place update and verified against the installed APK hash. That
installation and its save-preservation checks are documented separately.

The Scene Replay Android probes ran without the Activity, so they do not
constitute physical gesture testing of the final APK. Desktop DC2PC and
DC2TY checks used installed game data; DC2DM used fixtures, and DC4 asset
checks were skipped because `DC4_TEST_DATA` was unset. ARMv7, API 21/22,
physical 16 KB page devices, and full playthroughs of every supported game
were not exercised for this release.

The release excludes unrelated build-tool edits, cleanup work, generated
save fixtures, player saves, and temporary outputs.

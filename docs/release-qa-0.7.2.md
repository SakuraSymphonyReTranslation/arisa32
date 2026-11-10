# Arisa32 0.7.2 release verification

Verified on 10 and 11 November 2026 (Asia/Bangkok). Android versionName is
`0.7.2`, versionCode is `20`, and the package remains `org.dc4engine`.

## Packages

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.7.2.apk | 16,539,163 | `a9e2b50fe08e4d1da35dc03b364704fc5d63664e353abe84b29af53eb7da5108` |
| Arisa32-v0.7.2-debug.apk | 17,534,173 | `a6958abb4e748dd9ddb76e1c6405c70556c77a206a87cc97623c01aeda0da223` |

Both APKs pass ZIP integrity, v1/v2 signature verification, and
`zipalign -c -P 16 -v 4`. Their signing certificate matches earlier releases:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
Each contains ARM64 and ARMv7, requires API 21, targets API 36, and extracts
compressed native libraries. Only the debug APK is debuggable.

All ARM64 ELF load segments have at least 16 KB alignment. ARMv7 engine and
FFmpeg libraries also have 16 KB alignment; the NDK's ARMv7 `libc++_shared.so`
uses 4 KB alignment. The 16 KB compatibility check applies to ARM64.

Packaged fonts, license notices, and the DC2TY scenario-title table match
their source bytes. Checks confirm the DC2TY profile and updater classes,
the new vgmstream notice, and the absence of QA classes, game scripts,
CRX/CRM artwork, and save companions. There is no `assets/fonts/default.ttf`.

Android builds use Android Studio JBR 21.0.8, Gradle 9.0-milestone-1,
AGP 8.5.2, NDK 27.3.13750724, and two native compiler jobs. A temporary
Gradle init script selects the tracked AGP version. The unrelated local
AGP 8.13.2 edit is excluded from the release. SDK XML, compileSdk support,
deprecated API, and Gradle warnings are recorded in the build log.
The combined release/debug build completed all 87 tasks in 19m 10s.

## Source and updater checks

- The existing desktop Ninja Release build completed all 68 steps with
  MSVC x64 14.44.35207, CMake 3.31.6-msvc6, and Ninja 1.12.1.
- All 44 CTest tests passed in 352.50 seconds. Installed DC2TY data was
  enabled, including profile, save, menu, choice, transition, eyecatch,
  voice, and XPCM checks.
- Java profile checks passed 619 assertions, including all 129 bundled
  scenario titles against the original DC2TY executable.
- Save export checks passed 319 assertions.
- The release-note parser passed 179 checks against the localized release
  documents, including the final v0.7.2 text.
- Download checks passed 249 assertions across 48 scenarios.
- APK selection passed 143 checks with the original v0.7.1 fixture and
  another 143 with the same fixture using v0.7.2 filenames.
- A snapshot of 139 source and asset files remained unchanged during the
  builds. Staged whitespace checks pass.

The canonical release filename is `Arisa32-v0.7.2.apk`. Both APKs include
both supported ABIs; the debug package uses the `-debug.apk` suffix so
current and older updaters select the normal release package.

The Indonesian and English notes have matching features and limitations.
Each has one localized H1 and a language selector. The GitHub release body
matches the Indonesian document except for its absolute English link.
Report links resolve to files included in the release tag. The language
documents are published to `main` before the GitHub release, as required
by the app's localized-note downloader.

## Verification scope

Feature evidence remains in the DC2TY and DC2DM reports, including
[save compatibility](dc2ty-save-compatibility.md),
[date-change artwork](dc2ty-eyecatch-fix.md), and
[voice decoding](dc2ty-voice-analysis.md).
The original PC save reader/loader checks use isolated emulation and stop
before full asset and scene restoration. They do not establish a complete
PC cross-save playthrough. Unsupported native chip and background/pan
states are rejected; dialogue voice playback resumes from the beginning.

This release verification uses host tests and package inspection. It does
not install these version-code-20 APKs on a phone or capture the phone
screen. ARMv7, API 21/22, and physical 16 KB page devices were not exercised
for this release. Earlier development-device results remain documented
in their feature reports.

The commit excludes generated save fixtures, player saves, temporary build
outputs, and unrelated local cleanup scripts. A visual review of the 15
new PNG reports found game imagery and technical annotations. A limited
credential-pattern scan of 92 candidate text files found no matches.

Temporary build, package, desktop-test, and JVM logs are kept in task
directories under Windows Temp. The APKs and `SHA256SUMS.txt` are the
public distribution artifacts.

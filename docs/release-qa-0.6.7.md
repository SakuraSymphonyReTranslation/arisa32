# Arisa32 0.6.7 release verification

Release verification completed on 2026-11-05 for the work from November 4.
Android versionName is `0.6.7` and versionCode is `14`.

## Packages and build

| Artifact | Bytes | SHA256 |
| --- | ---: | --- |
| Arisa32-v0.6.7.apk | 13,309,353 | `2678be66c09e82602d919f4274171d9818d8b6ab8835f21cc29a9a0da59aacdd` |
| Arisa32-v0.6.7-debug.apk | 14,248,863 | `bf76dc707a106d3646301024b596a6cddb4bf8673f83bae5eeee0bf6d0cc584c` |

Both APKs match the final Gradle outputs and pass ZIP integrity and v1/v2
signature verification. Their manifests report `org.dc4engine`, version
0.6.7/code 14, minSdk 21 and targetSdk 36. Only the debug APK is debuggable.
Both use the same certificate as the published 0.6.6 APK:
`6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.

Package checks confirm arm64-v8a and armeabi-v7a, the engine/C++/FFmpeg
libraries, the new GLES/Surface/viewport classes, and the existing updater.
Neither APK contains the QA instrumentation classes, game scripts, artwork
archives or Arisa32 save files. `dist-v0.6.7/` contains the two APKs and
`SHA256SUMS.txt`. Package and source hashes, signature logs and manifest logs
are retained locally in `build/release-0.6.7/`; generated artifacts are not
committed.

The final release/debug build passed in 4 minutes 28 seconds using Gradle
9.2.0, Android Studio's JDK, NDK 27.3.13750724 and tracked AGP 8.5.2. A local
init script selects that AGP version while preserving an unrelated local
8.13.2 edit outside the release commit. Existing SDK, manifest-package and
deprecation warnings did not fail the build. The final build ran after the
JNI correction described below. Evidence:
`build/release-0.6.7/android-build-final.log`.

The release engine library hashes are:

- arm64-v8a: `684d9f4e394090eadf0f4e6930532d49c22cbe33acb04046348774f3f7a32f46`
- armeabi-v7a: `9672128dd56b57832687bb2449eb624bd6a94e611ee95b8501653a5388d66f40`

## Release regression results

| Check | Result | Local evidence |
| --- | --- | --- |
| Complete desktop CTest suite | PASS: 25/25 in 87.31 seconds, including COMU, text, scene, effects, audio, routes and saves | `build/release-0.6.7/ctest.log` |
| Release-note parser and translations | PASS: 139 checks, including both complete 0.6.7 documents | `build/release-0.6.7/release-notes-test.log` |
| Display preferences | PASS: 20 checks of defaults, limits, persistence and migration | `build/release-0.6.7/display-preferences.log` |
| Screen preferences | PASS: 10 checks of defaults, independent game settings, persistence and malformed values | `build/release-0.6.7/screen-preferences.log` |
| APK verification | PASS: both package variants, ABIs, version, signatures, production classes and checksums | `build/release-0.6.7/packages.json` |

The release audit found that JNI still forced default MS Gothic and explicit
`MSGOTHIC.TTC` selections to size 22, although the UI and scene renderer
already supported 18 through 22. `dc4ConfigureDisplay` now retains the selected
size with the same 18 through 22 clamp for every font. Changes to visual
effects during an active game retain their existing immediate behavior;
font-size preferences still apply on the next game launch.

The final build includes this correction. The scene tests compare rendered
frames at the supported sizes and check reload/save behavior, while the
earlier device checks verify the size selector. These are separate checks;
this release audit does not claim a new interactive test through the final
JNI game-launch path. The earlier FXAA report now records that distinction.

## Feature evidence from November 4

| Area | Recorded result and report |
| --- | --- |
| COMU custom-font readability | 505 supplied messages; 12 desktop/device comparisons plus two manual-spacing and five MS Gothic baselines with zero differing phone-region color components. See [custom-font validation](comu-custom-font-validation.md). |
| COMU patch compatibility | Manual spacing, Sakuramusubi word separators, 11 romanized senders and save/restore checks. See [manual-spacing validation](comu-text-validation.md) and [Sakuramusubi validation](comu-sakuramusubi-validation.md). |
| Legacy FXAA path | 227 GLES assertions on Android 16, minimum-API-21 lint, 391 combined graphics checks and 754 display-setting checks. See [FXAA validation](fxaa-legacy-validation.md). |
| Fit/Stretch | 360 screen checks and existing 431 navigation checks. See [screen-mode validation](screen-mode-validation.md). |
| Visual effects | Native effects, PC save import/export and recorded script replay checks. See [visual-effect validation](dc4-visual-effects-validation.md). |
| Voice, sprite priority and progression | Audio/backlog/scene checks and 256 ending-flag combinations per game. See [voice/layer QA](fix-qa-dc4-voice-layer-20261104.md) and [progression notes](dc4-progression.md). |

Those reports identify their feature-build APKs and evidence. Their APK and
library hashes are not the final 0.6.7 hashes above. The last runtime change
during release preparation is the JNI font-size correction.

## Scope and limitations

Device checks used an I2219 running Android 16/API 36. The GLES path was
forced on that device. Direct execution on Android 5 through 12 and native
armeabi-v7a execution remain unverified; successful builds and API checks do
not establish compatibility with every older graphics driver. No FPS or
battery-life claim is made.

COMU checks cover the supplied patch corpus and font, not every translation
or a complete PC playthrough. Old saves containing incorrect COMU senders or
voice references are not automatically migrated. Load a point before the
affected conversation to record new state. Progression tests extend coverage
of the existing 0.6.5 logic; the release does not erase completed progress.
DC2 Dearest Marriage remains beta.

The older Java archive harness has a documented preexisting failure at
`wrong game rejected` because the current importer skips its manifest. This
release does not claim that harness passes; the independent preference tests
above pass. See [Android test instructions](../tests/android/README.md).

## Release copy delivery gate

The user's requested changes and the established bilingual release format
provide the direction. Feature reports above retain their own UI and renderer
review evidence; this gate covers release metadata and Markdown copy.

| Item | Status and evidence |
| --- | --- |
| Hard Gate: R-02, R-17, R-18, R-23, R-24 | PASS: new release copy uses direct descriptions, verified counts and explicit artifact names, with no em/en dashes, invented people or assets. Both translations exist; language, report and download links use the intended version. |
| Purpose-Gate | PASS: existing headings and parallel feature lists let readers find changes and downloads. This release step introduces no visual decoration, controls or layout system. |
| Liveliness and R-37 | PASS for document scope: both languages retain the repository's factual release style; download labels identify the APK variants. No new visual direction or motion is introduced. |
| C-1 through C-5, R-15, R-16, R-35, R-36, R-38 | PASS: both translations cover the same behavior, limits and installation steps; parser, build and package results are recorded above. The remaining launch-path and old-device test limits are explicit. |
| Remaining visual consistency locks | PASS for document scope: release packaging changes no application palette, components, interaction or visual assets. The feature reports cover the application changes included in this release. |

Copy audit: the notes describe specific changes without marketing filler.
Names, version numbers, counts and compatibility limits come from source or
the linked validation reports. Both new translations contain no em/en dashes.

# DC2PC CG Mode validation, 8 November 2026

The DC2PC viewer now follows the original script between thumbnail groups, displays tall CGs through the original 800x600 viewport, and renders opened previews from the decoded source at the host bitmap size. The verified APK is installed on the I2219 phone. All measured saves and per-game settings remained unchanged during installation.

The reference is the original DC2PC interface and the two recordings supplied by the user. The continued session retains ENERGY 2 / RHYTHM 1 / MOTION 2: the selected CG is the focal point, gallery organization follows the game, and motion reveals the authored portion of tall artwork. Original assets supply the palette, typography, spacing and visual identity.

The implementation is limited to the DC2PC profile. The working tree also contains earlier DC2DM font and transition work; those changes are included in the APK but are not new CG Mode changes.

| Behavior | Result and reason |
| --- | --- |
| Advance | Follow each script's `cg_next` successors, including movement between thumbnail groups. Locked CGs are skipped. |
| Route end | Wrap only at the original script's cycle boundary. There are 16 cycles, including four distinct cycles in `cg_other`; this is not a promise that the complete gallery never wraps. |
| Tall CG | Apply raw opcode 0x62 mode 1 for the initial crop. The first advance starts mode 0 movement; ordinary clicks during movement are ignored, and a later advance opens the next CG. |
| Movement | Respect signed offsets and effect-speed steps of 2, 4, 8 or 16 source pixels per 17 ms simulation interval. Both directions and non-600-pixel distances are supported. |
| Reveal | Match the native accumulated blend at the four effect speeds. The reveal and the pan retain separate input stages. |
| Preview detail | Sample the decoded source into the requested host bitmap. This removes the shared 720p menu intermediate from the opened preview; it does not create detail beyond the source image. |
| Fit and Stretch | Use the existing host viewport and input mapping. Tall artwork stays cropped to the authored viewport while it moves. |
| Rendering cost | Cache horizontal Q8 samples so a vertical pan recomputes only the vertical interpolation. Cache invalidation covers asset changes, dimensions, horizontal offset, Back and game reload. |
| Memory | Keep the current and previous DC2PC CG needed for transitions, and release the row cache on preview exit or reload. |

Evidence is stored under `E:/Games/Adv32roid/build/dc2pc-cgmode-20261108/`.

| Check | Evidence |
| --- | --- |
| Desktop suite | `desktop/desktop-tests.log`: 34/34 tests passed after rebuilding the optimized source. |
| Targeted DC2PC test | `gallery-optimized-final.log`: 2,239 checks, zero failures, including synthetic cases, sparse-unlock boundary navigation for all 16 cycles, and numeric initial/middle/end crop checks for all 66 tall CGs. |
| Test count | The earlier 2,245-check run enabled six optional synthetic capture draws. The final 2,239-check run omitted capture export; it did not omit navigation or crop checks. |
| Detail and invalidation | `tests/dc2pc_gallery_test.cpp` checks exact source-center pixels at 3200x1800, resizing, bilinear samples, invalid output dimensions, cancelled touches, pan speeds, fade steps, Back, sparse unlocks and reload with changed source content. |
| Other profiles | `other-profile-optimized-comparison.json`: all 60 captures match the pre-DC2PC baseline byte for byte, comprising DC4 9, DC4PH 9 and DC2DM 42. |
| Android final native library | `device/optimized-01/result.log`: 80 synthetic checks, zero differences. The probe uses the library extracted from the delivered APK. |
| Earlier original-data Android run | `device/fixed-01/result.log`: 56 checks passed before the row-cache optimization. Original artwork produced numeric evidence only. The final optimized Android run uses synthetic artwork. |
| Synthetic visual review | Inspected `device/optimized-01/synthetic/captures/fit-pan-initial.png` and `stretch-pan-middle.png`: centered crops and intended viewport stretching, with no unexpected clipping or displaced bars. These are probe-generated Android Canvas captures. |
| Source integrity | `pc-save-optimized-verification.json`: all 14 PC save files match the previous baseline. Before/after probe manifests also show zero changes to the 14 PC and 19 phone save files. |
| Packaging | `apk-verification.json`, `apk-signature.log`, `apk-alignment.log`: both Android ABIs, expected signing identity, archive integrity, alignment and bundled font/license verified. |
| Installation | `installation-verification.json`: `adb install -r` succeeded, installed APK hash matches, 14 PC saves and 19 phone saves match before/after, and `library.properties` hash is identical. |
| Patch hygiene | `git diff --check` passed. Git reports existing LF/CRLF normalization warnings. |

The optimized phone probe exercised each affected interaction in Fit and Stretch:

| Action | Observed result |
| --- | --- |
| Open first thumbnail | Enter preview and use the HD JNI path. |
| Advance through IDs 0 to 5 | Continue across the first, second and third thumbnail groups. |
| Wait on initial tall image | Keep the initial crop until advance. |
| Advance tall image | Start movement and show an intermediate crop. |
| Advance during movement | Stay on the current CG without snapping to the endpoint. |
| Wait at endpoint | Hold the final crop. |
| Advance after movement | Open ID 6. |
| Back | Restore the gallery and disable the HD preview path. |
| Touch mapping | Map the viewport center to native coordinates; Fit bars reject input. |

The following times measure a native JNI gallery render into an Android bitmap while the synthetic pan is active. Each row uses 20 samples on the same I2219 device. The before measurement uses the completed navigation/rendering fix before the row-cache optimization. The measurements include cache preparation and bitmap copies; they exclude display presentation and are not a measured screen frame rate.

| Bitmap | Before mean | Optimized mean | Optimized median | Optimized p95 | Optimized maximum |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1920x1080 | 86.180 ms | 23.961 ms | 18.273 ms | 40.939 ms | 82.661 ms |
| 2416x1359 | 127.092 ms | 42.626 ms | 39.824 ms | 60.970 ms | 93.572 ms |

Sources: `device-fixed-01.log` and `device-optimized-01.log`. The larger bitmap is the rounded 16:9 host bitmap used for Stretch on the 2408x1080 surface. These samples show reduced rendering cost, but do not establish sustained 60 FPS. The synthetic fixture intentionally omits optional `start.mes` and message-window assets; their diagnostics are recorded in the log and the assertions pass.

The previous recording analysis decoded all 1,336 Arisa32 frames and 1,794 PC frames and computed per-frame differences. Visual review sampled the recordings; it did not manually inspect every frame. Original-image visual inspection stopped at a content restriction in the earlier session. This continuation displayed only synthetic test patterns. Recording limits, timestamps and source hashes are documented in `video/video-analysis.md`; script and native opcode evidence are in `reference/reference-report.md` and `reference/native-raw62-findings.md`.

The final device test runs the native library and production `ScreenViewport` code in an isolated probe. It does not certify the complete app UI or every original CG on the physical display. Desktop original-data checks cover route boundaries with sparse unlocks and every tall CG; they do not click through all 1,336 CGs consecutively. Existing desktop keyboard mappings were inspected, not replayed through the live SDL window in this continuation.

Delivered package: `org.dc4engine`, version 0.7.0/code 17, debug APK with arm64-v8a and armeabi-v7a libraries.

APK: `E:/Games/Adv32roid/build/dc2pc-cgmode-20261108/Arisa32-v0.7.0-dc2pc-cgmode-debug.apk`

SHA-256: `553957b4a3f3a919563446a1e4cae37852bdf7a8739e3dffe209d1096a7e8e81`

The following antislop delivery gate applies to the changed DC2PC rendering and navigation, synthetic validation, and this report. It does not claim a new accessibility audit of unchanged game menus. The primary agent reviewed the gate against the source, test results and synthetic captures.

| Gate item | Status and evidence |
| --- | --- |
| R-02 | PASS: authored report and new comments use plain punctuation. |
| R-03 | PASS: Fit/Stretch viewport and input checks passed; reviewed synthetic crops have the expected bounds. |
| R-17 | PASS: test counts, dimensions and timings identify their logs and limits above. |
| R-18 | PASS: no testimonials or invented identities were added. |
| R-23 | PASS: original game artwork is the user-selected reference; generated patterns are labeled synthetic and used only for tests. |
| R-24 | PASS: the route tests resolve the affected gallery destinations and Back action. No external navigation was added. |
| R-25 | PASS within scope: the change introduces no text, text color or text background. Existing menu contrast was not re-audited. |
| R-26 | PASS: opening, advancing, movement barriers and Back have recorded behavior in the interaction table. |
| R-27 | PASS within scope: locked-only fixtures stay outside preview, invalid render requests are rejected, and missing images retain the existing error path. No new data-loading UI was added. |
| R-28 | PASS: no FAQ was added. |
| R-32 | PASS within scope: no keyboard handlers or focus visuals were changed; existing SDL Space/Enter and Escape mappings were inspected. Live keyboard testing is outside the evidence claimed here. |
| R-33 | PASS: runtime behavior is implemented in C++ source; test scripts prepare fixtures and collect evidence. |
| R-34 | PASS within scope: no theme or theme toggle was introduced. |
| R-35 | PASS: both Android ABIs built, desktop and Android probe tests passed, and the affected interaction results are recorded above. |
| R-36 | PASS: performance is reported as measured JNI time with sample size and presentation limits. |
| R-37 | PASS: the original PC behavior supplies direction, with retained ENERGY 2 / RHYTHM 1 / MOTION 2. |
| R-38 | PASS: original assets and scripts supply game behavior; synthetic patterns are identified as test data. |
| R-01 | PASS: original palette retained; no decorative gradient or glow added to the product. |
| R-04 | PASS: no icon set or decorative glyph added. |
| R-06 | PASS: the DC2PC change does not alter typography. |
| R-07 | PASS: test patterns are fixtures, not new product backgrounds. |
| R-08 | PASS: no decorative arrows added. |
| R-09 | PASS: no badges added. |
| R-10 | PASS: no glass effects added. |
| R-12 | PASS: no shadows added. |
| R-13 | PASS: no glow added. |
| R-14 | PASS: gallery grouping follows the authored script and existing thumbnail layout. |
| R-19 | PASS: motion exposes the authored tall-image crop; endpoint waits and click barriers are tested. |
| R-22 | PASS: no product illustrations generated. |
| Liveliness: dials | PASS: ENERGY 2 / RHYTHM 1 / MOTION 2 is stated and inherited from the reference direction. |
| Liveliness: consistency | PASS: authored grouping remains uniform and motion is limited to scripted pan/reveal behavior. |
| Liveliness: focal point | PASS: the opened CG occupies the content viewport. |
| Liveliness: spacing | PASS: existing gallery layout and Fit bars retain their framing function. |
| Liveliness: accent | PASS: original menu selection treatment is retained; no competing accent added. |
| Liveliness: identity | PASS: game resources and script-defined transitions retain DC2PC's identity. |
| Liveliness: design read | PASS: original PC gallery direction and the same dials were declared in the earlier implementation session and retained here. |
| C-1 | PASS: each changed behavior has a purpose in the first table. |
| C-2 | PASS: all changed interaction stages have explicit probe/test results. |
| C-3 | PASS: no template sections or product content were added. |
| C-4 | PASS within scope: sparse unlocks, dimensions, cancelled touches, pan speeds, Back and reload are tested; unchanged UI is outside the audit. |
| C-5 | PASS: claims cite recorded evidence and distinguish numeric checks, synthetic visual review and live UI coverage. |
| R-05 | PASS: original gallery organization is retained. |
| R-11 | PASS: no component geometry or border radius changed. |
| R-15 | PASS: no generic CTA introduced. |
| R-16 | PASS: report language describes measured behavior without marketing claims. |
| R-20 | PASS: original artwork and authored routes remain specific to DC2PC. |
| R-21 | PASS: no theme changes introduced. |
| R-29 | PASS within scope: the original game palette is retained. |
| R-30 | PASS: the user explicitly requested the original game's behavior as the reference. |
| R-31 | PASS: reasons for navigation, crop, filtering, caching and motion are recorded above; unchanged typography and layout retain the game's direction. |

Comment review passed for the DC2PC changes: comments explain native blend cadence, authored cycles and the Q8 constraint without decorative banners. Copy review passed: this report states its evidence, avoids invented results, and distinguishes implementation coverage from the untested parts of the full app.

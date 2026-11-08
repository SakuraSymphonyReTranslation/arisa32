# COMU Pretendard padding validation, 2026-11-08

The DC4/DC4PH padded-patch COMU renderer now uses Pretendard JP Bold's natural letter advances. Glyph size remains 16px, each patch space remains 8px, and the original byte-28 split, baselines, and two-line clipping area remain in place. Narrow letters no longer receive the same advance as wide letters.

The fix requires both the selected font's internal `PretendardJP-Bold` identity and a Latin message containing standalone A5 patch spaces. It does not inspect game-folder names or font filenames. Tests cover a relocated game, a renamed Pretendard file, and an MS Gothic file misleadingly named `PretendardJP-Bold.ttf`.

The reference direction is the user's existing PC-style PhoneChat layout with their chosen font. ENERGY 2 / RHYTHM 1 / MOTION 1: retain the game artwork and uniform message rows, and add no animation. Natural glyph advances improve readability; fixed space cells preserve the patch's authored spacing; fixed size and baselines retain the original placement.

All evidence below is under `build/comu-pretendard-20261108/` unless another path is given.

## Recording and patch evidence

All 2,922 frames of `dc4ph_and_dc4_using_pretendard_JP_BOLD.mp4` were sequentially decoded and numerically analyzed. There are 14 visible phone states and 16 show/add/scroll/hide events. Visual inspection covered every visible state in contact sheets, full-size renderer comparisons, and selected before/at/after transitions. It did not cover every full frame manually. See `video/report.md` for timestamps and source hash.

The earlier transition JPEGs were mislabeled by OpenCV random seeking on variable-frame-rate video. Fresh sequential decoding verified all 16 event PNGs and generated the corrected `video/phone/sequential-transition-*.png` sheets.

| Effective patch corpus | Messages | A5-space messages | Padded Latin path | Sakuramusubi prose path |
| --- | ---: | ---: | ---: | ---: |
| User DC4 | 498 | 498 | 497 | 0 |
| User DC4PH | 505 | 505 | 504 | 0 |
| Sakuramusubi | 505 | 0 | 0 | 505 |

Each user corpus has one punctuation-only `?????` message that retains the old path. Sakuramusubi has 484 DEL-separated messages and 21 short messages. Its classification, 18-22px Pretendard sizing, wrapping, and 152 three-line layouts remain unchanged. The patch inventory resolves overlays over base MES files before extracting COMU opcode 0x6D bodies. Paths in the inventory identify samples only; they are not runtime detection rules.

## Validation

| Check | Result | Evidence |
| --- | --- | --- |
| Complete desktop build and CTest | 34/34 passed | `desktop-build-all.log`, `desktop-tests.log` |
| DC4 full corpus | 498 messages; 0 failures; maximum line advance 226px | `desktop-dc4-verified.log` |
| DC4PH full corpus | 505 messages; 0 failures; maximum line advance 228px | `desktop-dc4ph-verified.log` |
| Other-font and Sakuramusubi baseline comparisons | 22/22 control comparisons identical; the two padded Pretendard samples changed as intended | `baseline-comparison.json` |
| Final Android library on connected device | Both corpus runs passed with Android Java windows-31j decoding | `android/user-dc4-result.log`, `android/user-dc4ph-result.log` |
| Android versus desktop phone pixels | 212 DC4 + 214 DC4PH captures identical; 0 differing pixels | `android/dc4-comparison.json`, `android/dc4ph-comparison.json` |
| Android package | Both arm64-v8a and armeabi-v7a built; signature, archive, alignment, font/license and native-library checks passed | `android-build.log`, `apk-verification.json`, `apk-signature.log`, `apk-alignment.log` |
| Installed APK | Installed SHA-256 equals the tested artifact | `installation-verification.json` |
| Preserved files | 27 DC4 + 49 DC4PH + 49 Sakuramusubi save/profile files, library.properties and both preference files unchanged | `device-before-install.json`, `device-after-install.json` |

The CTest COMU entry exercises the fixed font and classifiers. The separate full-corpus invocations additionally supply Pretendard and both patch corpora. The Android probe links against `libarisa32.so` extracted from the exact delivered APK and writes test fixtures under `/data/local/tmp`; it does not load or overwrite player saves. Its manifest records the APK, library, font, test-source, and corpus hashes.

One test assertion needed correction during continuation: the fixture includes ordinary `STOP` dialogue, so changing the dialogue font size changes the full frame. The test now verifies that the full frame changes while the entire phone rectangle remains identical. Production behavior did not require another change.

The following command-driven interactions passed for both MES versions 0xAAB6 and 0xABB6. These are engine/probe tests, not a claim of tapping every control in the live Android UI.

| Interaction | Observed result |
| --- | --- |
| Add a sixth message | Only the newest five rows remain visible. |
| Hide, show, then destroy COMU | Hide retains the timeline; show restores it; destroy clears it. |
| Display a stamp | Stamp appears in its existing slot. |
| Capture and restore state | Restored phone pixels equal the original. |
| Commit group and advance timed script | Phone remains through the tested preloaded/wait stages, then clears at the original script transition. |
| Change ordinary dialogue size | Ordinary dialogue responds; padded COMU stays 16px with identical phone pixels. |
| Select other faces or Sakuramusubi text | Existing font/layout path remains active. |

## Existing limits

Five DC4 messages and two DC4PH messages contain an explicit LF after the native byte-28 break. The resulting third row is still clipped by the existing two-row buffer. Their zero-based corpus indices are DC4 37, 67, 71, 80, 301 and DC4PH 200, 410. This continuation preserves those authored line breaks and does not reflow, enlarge, or rewrite the patch. None of the 1,001 padded Latin messages exceeds the tested 236px horizontal advance limit.

The recording verifies the reported old Arisa32 appearance. A separate native Windows recording was not supplied for this continuation, so exact Windows glyph-pixel parity is not claimed. The phone pixel comparison validates the production native renderer on Android; it does not certify every host scaling mode or the complete live app UI. No profile settings were changed to perform it.

## Delivered package

`org.dc4engine`, version 0.7.0/code 17, debug APK:
`build/comu-pretendard-20261108/Arisa32-v0.7.0-comu-pretendard-debug.apk`

SHA-256: `ca411d7811dfda80f0a69b995a7704a649dd8476090ab6cda282ca6fd88ad597`.

The installed artifact contains the preceding DC2DM and DC2PC work as well as this COMU fix. Source changes outside this COMU continuation were preserved.

## Antislop delivery gate

The primary agent reviewed this gate against the changed spacing, test results, source diff, original phone artwork, before/after images and this report. PASS is scoped to this change; it does not certify unchanged game UI or remove the explicit-LF limitation above.

| Item | Status and evidence |
| --- | --- |
| R-02 | PASS: new report and comment text use plain punctuation. |
| R-03 | PASS within scope: all measured horizontal advances fit; the phone bounds and two-row clipping are retained. Existing explicit-LF clipping is disclosed above. |
| R-17 | PASS: counts, dimensions and hashes come from the named logs and manifests. |
| R-18 | PASS: no testimonials or invented identities added. |
| R-23 | PASS: original game artwork and the user-selected font supply the visual assets. |
| R-24 | PASS: no navigation or destinations added. |
| R-25 | PASS within scope: text retains its white fill, black outline and existing bubble background. No colors changed; this is not a new contrast audit of the game. |
| R-26 | PASS: affected command-driven behaviors have recorded results in the interaction table. |
| R-27 | PASS within scope: empty groups, hide/show, clear, missing textbox font and state restoration are covered by existing scene checks. No loading UI added. |
| R-28 | PASS: no FAQ added. |
| R-32 | PASS within scope: input mappings and focus visuals are untouched; tests drive the existing scene API. Live keyboard navigation was not re-audited. |
| R-33 | PASS: spacing is implemented in C++ source; scripts only build, prepare fixtures and collect evidence. |
| R-34 | PASS within scope: no theme controls or theme-dependent styles changed. |
| R-35 | PASS: 34 desktop tests and both final-library Android probes passed; engine interaction coverage and live-UI limits are recorded above. |
| R-36 | PASS: the report distinguishes measured pixels, visual samples and untested behavior. |
| R-37 | PASS: the user's original layout and chosen font supply direction; ENERGY 2 / RHYTHM 1 / MOTION 1 is explicit. |
| R-38 | PASS: patch text and game assets are real inputs; test-only strings are identified as fixtures. |
| R-01 | PASS: original palette retained; no decorative gradient added. |
| R-04 | PASS: no icons added. |
| R-06 | PASS: natural advances serve readability in the user's chosen Pretendard face. |
| R-07 | PASS: no backgrounds added. |
| R-08 | PASS: no decorative arrows added. |
| R-09 | PASS: no badges added. |
| R-10 | PASS: no glass effects added. |
| R-12 | PASS: no shadows added. |
| R-13 | PASS: no glow added. |
| R-14 | PASS: original five-row phone layout retained. |
| R-19 | PASS: no new motion; original message arrival and scrolling behavior retained. |
| R-22 | PASS: no illustrations generated. |
| Liveliness: dials | PASS: ENERGY 2 / RHYTHM 1 / MOTION 1 follows the existing phone view. |
| Liveliness: consistency | PASS: uniform bubbles and script-driven message updates retain the stated direction. |
| Liveliness: focal point | PASS: the dimmed scene leaves the phone conversation prominent. |
| Liveliness: spacing | PASS: 8px space cells preserve the patch's grouping and padding. |
| Liveliness: accent | PASS: original blue bubble outlines and sender artwork remain the existing accents. |
| Liveliness: identity | PASS: original phone skin and sender portraits retain the game's identity. |
| Liveliness: design read | PASS: the existing PhoneChat reference and dials were declared before completing the deliverable. |
| C-1 | PASS: the reasons for glyph advances, space cells, fixed size and baselines are stated above. |
| C-2 | PASS: no controls added; relevant scene actions have passing tests. |
| C-3 | PASS: no template sections added to the product. |
| C-4 | PASS within scope: mixed SJIS boundaries, relocated folders, renamed fonts, alternate faces and both MES versions are tested. |
| C-5 | PASS: every result has evidence, with pre-existing limits and visual-review coverage stated. |
| R-05 | PASS: original conversation structure retained. |
| R-11 | PASS: no component shapes or radii changed. |
| R-15 | PASS: no CTA text added. |
| R-16 | PASS: report describes concrete behavior without marketing claims. |
| R-20 | PASS: original game artwork and authored text retain product identity. |
| R-21 | PASS: no theme changes introduced. |
| R-29 | PASS within scope: the existing game palette is preserved. |
| R-30 | PASS: the original game is the user's explicit reference. |
| R-31 | PASS: the implementation retains authored positioning while removing forced letter spacing; no unrelated visual choices introduced. |

Comment review: the new production comment explains the A5-cell constraint. Copy review: this report names evidence and limits, avoids invented facts, and uses a technical reporting tone.

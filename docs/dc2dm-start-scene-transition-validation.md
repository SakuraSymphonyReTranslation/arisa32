# DC2DM Start and scene-transition validation

This change follows the supplied Da Capo II Dearest Marriage recordings and
the installed game's scripts. New transition handling is gated to ClassicDM
MES version `0x9D72`; the Start confirmation exception additionally requires
the DC2DM title menu. Other profiles retain their existing behavior.

## Start progression

| Saved completion flags | Start behavior |
| --- | --- |
| Graduation Days unfinished | Native Yes/No confirmation, then Graduation Days |
| Graduation Days finished; Married Life unfinished | Direct selector: Graduation Days and Married Life |
| Graduation Days and Married Life finished | Direct selector: Graduation Days, Married Life, and Dear to |

The flags are bank `100[1]` and `100[2]`. `SCESELECT.mes` checks Graduation
completion before building the selector; Married completion adds its third
story. If an inconsistent save has only the Married flag, Graduation still
controls the initial gate. Completing Graduation naturally continues into
Married Life; completing Married Life continues into Dear to.

The PC Start recording demonstrates the fully unlocked state. The script
establishes the Graduation gate and the two/three-story selector. Retaining
the initial Yes/No confirmation follows the user's explicit request; the
inspected native title script does not itself establish that confirmation.
The frame audit covers every frame of both Start recordings:
435 Android frames and 221 PC frames. The separately attached PC recording is
byte-identical to the PC file in Downloads. See
[the Start analysis](dc2dm-start-frame-analysis.md).

## Scene transitions

The classroom recording contains 306 frames. The source message window hides,
the outgoing scene blends into white through `MSK\\MASK02B`, and the daytime
classroom is revealed through a second use of that mask. The VM must wait for
each effect even though the script has no separate WAIT between them. The
message window returns at the following dialogue. See
[the frame analysis](dc2dm-scene-transition-frame-analysis.md).

Implemented DC2DM modes are global dissolve 0, fade-in 1, fade-out 2,
accelerating feather reveal 17 with selector 1, and authored mask modes 20/21.
Mode 20 has no literal calls in the inspected scripts but shares the decoded
mask contract. Masks are consumed after use; a later maskless 21 falls back
to dissolve, as the native engine does. A fade-out holds black at its endpoint.

The native loop crops the 1024-by-600 mask artwork to its top 1024-by-576
region. The Android scene scales that region to 1280-by-720. Mask values are
preserved, including the limited 0..68 range of the radial MASK01A. Transition
timing follows the executable's 34 ms ticks, speed tables, blend divisors, and
settle intervals. Recording wall-clock duration also includes rendering and
capture cadence and is not asserted to match those nominal ticks exactly.

Returning to the title now discards the DC2DM transition buffers, mask, and
script wait, including the selector's indefinite input wait. This cleanup
runs even when ordinary gameplay already allows Save/Load. It does not
advance the abandoned script or commit an interrupted minigame result.

The inventory covers 311 scripts, 12,522 literal transition calls, 16 used
modes, and five used mask assets. The native algorithm, asset shapes, counts,
and disassembly evidence are in
[the transition inventory](dc2dm-script-transition-inventory.md).

## Coverage limits

- Mode 16 tile-particle exits, flash/shake modes 3/4/6, and sprite-slot effects
  51/52 were identified but are not faithfully implemented by this change.
  Mode 16 is separate from the previously implemented circular Title effect.
- The port snapshots scene-only content before dialogue batches and at mode
  32. Native mode 32 is a barrier, not a pixel snapshot; more complex sequences
  still need comparison with native committed/pending scene state.
- The unlocked Android Title-to-selector host boundary still passes through
  black, whereas the PC recording shows a direct crossfade. This change fixes
  the conditional confirmation, not that separate host-boundary discrepancy.
- Device probes exercise the APK's actual ARM64 JNI library with real assets
  and isolated save copies. They are not a full Activity walkthrough, complete
  story playthrough, or an acoustic comparison of every voice clip. ARMv7 is
  built and packaged but has not run on a 32-bit device.

## Build configuration

The repository's AGP 8.13.2 edit is preserved. This machine's cached AGP 8.5.2
is selected only through the existing baseline Gradle init script for the
offline build. SDK/AGP compatibility and future Gradle deprecation warnings
remain visible in the build log; they did not stop compilation.

All probe saves stay in unique `/data/local/tmp/arisa32-dc2dm-save-*` fixtures.
Only asset directories are linked from the installed game. The player's PC
and phone SaveData folders are compared recursively against fresh SHA-256
baselines after validation and installation.

## Final verification, 8 November 2026

All paths in the evidence column are relative to the repository root.

| Check | Result | Evidence |
| --- | --- | --- |
| Desktop regression suite | PASS, 32/32 on the final core: 30 remaining tests, transition test, and relinked menu test | `build/dc2dm-scene-transition-20261108/full-desktop-tests.log`, `focused-tests.log`, `final-menu-test.log` |
| Java profile checks | PASS, 328 checks | `build/dc2dm-scene-transition-20261108/java-profile-checks.log` |
| Android build | PASS, ARM64 and ARMv7; final mask crop and forced-Title cleanup included | `build/dc2dm-scene-transition-20261108/android-build-mask-crop.log`, `final-source-state.json` |
| Packaged ARM64 library on I2219 | PASS, all nine phases | `build/dc2dm-save-validation/d4e6e2202b6e/result.log` |
| APK signature, alignment, manifest | PASS, v1/v2 signatures, 16 KB page alignment check, expected package/version/ABIs | `build/dc2dm-scene-transition-20261108/apk-signature.log`, `apk-alignment.log`, `apk-manifest.log` |
| Packaged native libraries | PASS, both libraries match their final staged build files byte for byte | `build/dc2dm-scene-transition-20261108/apk-native-libraries.json` |

The nine device phases are transitions; stage0, stage1, and stage2; each
stage after a separate-process restart; choices; and minigames-suit. The
runner loads the final APK's JNI library against the actual game assets.
It verifies Cancel/No/Yes at stage0, direct two/three-story selectors and
their Return paths at stages1/2, and persistence across restart. The choice
phase checks both recorded branches, normal/hover/pressed/cancel/outside
touches, voiced Anzu, and unvoiced Yoshiyuki/narration without stale replay.
The suit phase covers all hands, native exit/continue, and forced Title
without committing abandoned scores or leaving Load locked.

The transition phase verifies the recorded cover/white/reveal sequence,
directional feathered boundary, pause, VM barriers, transient-save rejection,
forced Title, and loading again. Desktop tests additionally exercise native
tick/blend arithmetic, mask crop, missing/consumed-mask fallbacks, selector
wait cleanup, native checkpoint round trip, and non-DC2DM versions `0xAAB6`,
`0xABB6`, and `0x9969`. No final build or test phase timed out.

Root inspected the final device captures at full resolution:
`start-confirm-stage-0.png`, `selector-stage-1.png`, `selector-stage-2.png`,
`transition-cover.png`, `transition-white.png`, `transition-reveal.png`,
`transition-after.png`, and `transition-forced-title.png`, all under
`build/dc2dm-save-validation/d4e6e2202b6e/captures`. The original prompt is
present only in stage0; stage1 has two stories and BACK; stage2 has three
stories and BACK. The classroom transition has the intended white cover,
soft reveal, and restored message window. LOAD is visibly enabled after
the forced return to Title.

## APK

- File: `build/apk/Arisa32-0.6.9-debug-DC2DM-start-transitions-20261108.apk`.
- Package: `org.dc4engine`, version `0.6.9`, version code `16`, debuggable.
- Size: 16,335,056 bytes; ABIs: `arm64-v8a` and `armeabi-v7a`.
- SHA256: `756834e7a53cbfb60007ac48c0a30fdd127c0f6f00b33b5c81c9f94e54a0a396`.
- Signing certificate SHA256: `6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
- ARM64 library SHA256: `4afd52936aa2ea3eec805e7c94aaa54ce6a8f3fcef725c4568566ef98647530d`.
- ARMv7 library SHA256: `9ac1e7a6552ae694c14c85f58f5b337e1b813e4c4cd63881b4f03272369ee521`.

## Device installation and save integrity

PASS: `adb -s 10DF1N05QN000H8 install -r` installed the final APK on the I2219
and returned `Success`. Package Manager reports version `0.6.9`, code `16`,
ARM64, and update time `2026-11-08 08:16:44`. The existing app was updated
without uninstalling it or clearing its data.

PASS: the installed `base.apk` SHA256 is
`756834e7a53cbfb60007ac48c0a30fdd127c0f6f00b33b5c81c9f94e54a0a396`,
identical to the final local APK and the APK used in the device probe.

PASS: recursive post-install comparisons against this task's fresh baselines
found all 16 phone save files and all 10 PC save files unchanged. There are no
added, missing, or changed files in either SaveData tree.

Evidence is in `build/dc2dm-scene-transition-20261108/installation.log`,
`installation-verification.json`, `installed-package.txt`,
`phone-saves-before.txt`, `phone-saves-after-install.txt`,
`pc-saves-before.json`, `pc-saves-after-install.json`, and `save-integrity.json`.

## Scoped Delivery Gate

Root performed this gate for the conditional DC2DM Start prompt and the
implemented scene transitions. The user's direction is the original PC UI
and behavior. Original bitmap artwork, typography, and palette are retained
under that direction; this is not an accessibility redesign or a claim of
full Activity coverage. The reference is the supplied recordings and native
scripts, established before implementation. Descriptive dials are ENERGY 2
(original pink controls and illustrated screens), RHYTHM 1 (fixed canvas and
menu layout), and MOTION 2 (scripted scene changes). The coverage limits above
remain part of this delivery.

### Hard Gate

| Item | Status and evidence |
| --- | --- |
| R-02, prose | PASS: this report uses factual descriptions and no em dash; game text is sourced content. |
| R-03, layout | PASS in the tested scope: full-resolution 1280 by 720 captures keep prompts, story rows, BACK, and dialogue within the game canvas. |
| R-17, statistics | PASS: test counts, frame counts, and hashes have the recorded sources listed above. |
| R-18, testimonials | PASS: no testimonials or invented people introduced. |
| R-23, assets | PASS: the requested PC presentation uses existing CRX/MSK assets; no replacement artwork created. |
| R-24, navigation | PASS: stage0 confirmation paths and stage1/2 story selectors/Return paths pass the device probe. |
| R-25, contrast | PASS for this change's scope: no new text colors or controls introduced; original bitmap art is retained by explicit user direction, without claiming WCAG certification. |
| R-26, controls | PASS: Start, Yes, No, Cancel, story choices, and Return have tested behavior. |
| R-27, states | PASS: transition tests cover missing masks, pause, completion, interruption, reload, and input-wait cleanup. |
| R-28, FAQ | PASS: no FAQ introduced. |
| R-32, keyboard/focus | PASS for the changed branch: it uses the existing SystemMenu action/selection path and native selector controls; no separate pointer-only widget or replacement focus styling added. Full Android keyboard traversal is not claimed. |
| R-33, implementation | PASS: production behavior is implemented in the C++ sources; analysis scripts only generate evidence. |
| R-34, themes | PASS: no theme toggle introduced; the original game appearance remains the requested reference. |
| R-35, execution | PASS: both Android ABIs built, 32 desktop tests and nine device phases passed; interaction captures are retained. |
| R-36, claims | PASS: source, build, runtime, and visual claims have separate evidence; unsupported effects and validation limits are stated. |
| R-37, direction | PASS: the user explicitly requested the PC game presentation and conditional prompt behavior. |
| R-38, content | PASS: menu names, flags, masks, and script behavior come from the actual game; no fictional content added. |

### Purpose-Gate

| Item | Status and evidence |
| --- | --- |
| R-01, gradients/glows | PASS: original pink art preserves the reference; feather gradients implement the native mask boundary. |
| R-04, icons | PASS: existing game controls retain their original action icons. |
| R-06, typography | PASS: existing bitmap labels and message font preserve the supplied PC presentation. |
| R-07, patterns | PASS: no decorative grid or dot pattern introduced. |
| R-08, arrows | PASS: no decorative arrows added to controls. |
| R-09, badges | PASS: no badges introduced. |
| R-10, glass | PASS: no new glass effect; original translucent dialogue artwork is retained. |
| R-12, shadows | PASS: no new component shadows. |
| R-13, glow | PASS: no added decorative glow or pulsing status effect. |
| R-14, cards | PASS: native story rows represent the available stories; no feature-card layout added. |
| R-19, motion | PASS: the white cover/reveal follows the scene-change script; pause and completion are tested. |
| R-22, illustration | PASS: backgrounds and character sprites come from the game assets referenced by the script. |

### Liveliness

| Item | Status and evidence |
| --- | --- |
| Explicit dials | PASS: ENERGY 2 / RHYTHM 1 / MOTION 2 describe the preserved reference above. |
| Consistency | PASS: reviewed captures keep a stable menu/canvas layout and scene-driven transitions. |
| Focal point | PASS: confirmation or story choice is the active menu decision; the scene is primary during transitions. |
| Structural space | PASS: original story-row spacing and message-box regions remain intact. |
| Accent | PASS: native selected-state artwork identifies the active choice. |
| Identity | PASS: original DC2DM pink artwork, sakura background, and bitmap story labels remain visible. |
| Design reference | PASS: supplied recordings and script audit established the reference before the implementation. |

### Craftsmanship and Quality Locks

| Item | Status and evidence |
| --- | --- |
| C-1, intention | PASS: prompt gating removes a redundant confirmation; mask geometry and timing follow native evidence. |
| C-2, completeness | PASS: tests exercise the changed confirmation, selection, Return, and interrupted-transition paths. |
| C-3, composition | PASS: existing regions serve the story selection, game scene, or dialogue. |
| C-4, resilience | PASS within tested scope: restart, pause, missing masks, cancellation, forced Title, and reload are covered. |
| C-5, evidence | PASS: test and visual claims are backed by logs and retained captures. |
| R-05, structure | PASS: the native game layout is retained without a template shell. |
| R-11, shapes | PASS: original prompt and story-control artwork is unchanged. |
| R-15, labels | PASS: Start, story names, Yes/No, and BACK keep their existing concrete meanings. |
| R-16, language | PASS: no marketing copy or invented product claims added. |
| R-20, identity | PASS: the original DC2DM assets remain specific to this game's presentation. |
| R-21, theme | PASS: original game artwork determines appearance; no arbitrary dark theme introduced. |
| R-29, palette | PASS: existing pink controls and authored scene artwork follow the requested game palette. |
| R-30, reference | PASS: the user explicitly requested this PC game presentation. |
| R-31, rationale | PASS: original artwork/layout preserve the reference; the conditional prompt follows the requested gate; transition timing and cropping follow native code. |

The UI supplement passes for the changed screens: original assets establish
hierarchy, tested controls provide behavior, and motion has a script-defined
purpose. The copy supplement passes for this report: numerical claims have
sources, initial-confirmation evidence is distinguished from selector script
evidence, and limitations remain explicit.

# DC2DM Pretendard JP Bold layout validation

Validated on 2026-11-08. DC2DM now uses the selected Pretendard JP Bold face's Latin advances and full glyph size in dialogue, speaker names, and choices. This corrects the uneven spacing and undersized letters visible in the supplied recording. The compatibility change requires both the DC2DM UI profile and the font's `PretendardJP-Bold` PostScript name. MS Gothic and other game profiles keep their existing rendering.

## Recording analysis

Source: the supplied `dc2dm_pretenderd_jp_bold.mp4` recording, SHA256 `752cf4fdcc24e7740ab5eac8266ec4a432e2c79e5387d32c8eac764e33c45006`.

All **5,603 frames** were decoded and scanned for dimensions, timestamps, and pixel hashes. The timestamps run from 0.000 to 114.358 seconds and increase strictly. After rotation, every frame is 1606 x 720; the game viewport occupies x169 through x1448. The scan found 5,521 unique viewport hashes and 3,237 unique textbox-region hashes. Visual inspection used 72 chronological samples plus detailed problem frames. This was an exhaustive pixel/timestamp scan with sampled visual inspection, not a claim that every frame was manually inspected.

| Frame | Time | Observation |
| --- | --- | --- |
| 100 | Early font selection | The selected face is Pretendard JP Bold. |
| 1500 | 34.473656 s | Wide Latin letters crowd their cells while narrow letters leave excessive gaps. The bright glyph cores measure 15 px for the name and 16/16/14 px for the three body rows, with 30 px between row starts. |
| 3000 | Dialogue | The name and body show the same uneven spacing. |
| 5477 | 109.973867 s | The choice screen begins. |
| 5500 | 110.738933 s | Button artwork is intact; labels are small and unevenly spaced. Bright glyph cores are 15 px and 11 px high, depending on descenders. |
| 5603 | 114.358 s | The choice screen remains visible at the recording's end. |

Compressed-video brightness masks exclude outlines and can also include background pixels. They support the diagnosis; they are not OCR or an automatic clipping proof. Backlog appears near frame 5280 but is outside this textbox/name/choice correction.

Evidence: [scan summary](../build/dc2dm-pretendard-layout-20261108/scan/summary.json), [all-frame measurements](../build/dc2dm-pretendard-layout-20261108/scan/all-frames.csv), [key-frame bounds](../build/dc2dm-pretendard-layout-20261108/scan/key-frame-ink-bounds.json), and [chronological overview](../build/dc2dm-pretendard-layout-20261108/scan/overview-3.png).

## Implementation and reasons

- Identify the face from font metadata, so an imported copy renamed `default.ttf` receives the same correction. Reloading MS Gothic or failing a new load resets the identity flag.
- Use proportional Latin advances with no forced tracking for DC2DM Pretendard. The former fixed grid was designed around the original font and distorted the replacement's spacing.
- Remove the translation path's `15/19` glyph shrink only for this face. Glyph proportions now match the selected font.
- Center speaker names by measured width. Fit choice labels using measured width and center them vertically using their glyph bounds inside the authored buttons.
- Measure each line's ink and ruby ascent before drawing. Large script styles and ruby receive enough line spacing; unusually tall blocks scale uniformly to fit within the textbox. Both axes use the same scale.
- Compute layout from the complete parsed block so typewriter reveal does not move existing text. Transform the wait marker with the fitted block; when fitting is unnecessary, its position reduces to the old calculation.
- Keep the original DC2DM palette, artwork, buttons, outlines, and authored interaction states. They establish the game's identity and keep dialogue and choices legible over the scene.

No general font rasterizer, default-font setting, or translation content was changed for this correction. Existing unrelated workspace changes were retained.

## Desktop verification

The targeted font suite passes **14 profile/font/language combinations** with native DC2DM UI assets, and the same 14 combinations with isolated fixture assets. It covers Latin/Japanese text, narrow and wide letters, descenders, word wrapping, script styles, ruby, typewriter reveal, long choices, hit testing, all three choice branches, and hover/pressed states. Font identity checks include a renamed import, reloading another font, and a failed load.

Tall-style fixtures include two large Latin lines, large text followed by ruby, and four large ruby/base pairs. Bright-ink bands remain separate, equal-size rows retain equal glyph heights, and full changed-pixel bounds stay above y710. The four-pair visual capture ends within the textbox. Faint antialiased outline pixels may touch between adjacent large lines; band assertions measure bright glyph cores while overflow assertions still include every changed pixel.

The frozen before/after comparison contains **132 image pairs**: all **20 DC2DM Pretendard** captures change as intended; all **112 control captures are byte-identical**. Controls cover DC2DM MS Gothic through stb and Windows GDI, and DC2PC/DC4 with both fonts. There are no unexpected differences.

The complete desktop suite passes **33/33 tests**, with zero failures, in 166.26 seconds.

Evidence: [targeted log](../build/dc2dm-pretendard-20261108/after/native-ui.log), [full suite log](../build/dc2dm-pretendard-20261108/after/desktop-tests.log), [comparison summary](../build/dc2dm-pretendard-20261108/comparison.json), [comparison rows](../build/dc2dm-pretendard-20261108/comparison.csv), [before/after visual](../build/dc2dm-pretendard-20261108/pretendard-comparison.png), and [four ruby lines](../build/dc2dm-pretendard-20261108/after/native-ui/dc2dm-pretendard-latin-large-ruby-four-lines.png).

## Android verification

The final APK builds for both `arm64-v8a` and `armeabi-v7a`. ZIP integrity, package/version, debug flag, v1/v2 signatures, alignment, packaged native-library equality, and bundled font/license bytes pass verification.

| Property | Value |
| --- | --- |
| Package | `org.dc4engine` |
| Version | `0.7.0`, code `17`, debug |
| APK bytes | 19,427,293 |
| APK SHA256 | `6bc13e7e923daa175b11f0427b23d808aa510aa9beb82d35c1c449b227c09d58` |
| arm64 library SHA256 | `933fdc72a6741895598abf1cfc3515764ed06ceab36dd475b3d6d2c86e8cb13f` |
| armeabi-v7a library SHA256 | `d288cef50a6738211589c52c588a847b89795b6dae63ef3665de5f69cb50b8e4` |

On the connected iQOO Z9x 5G / I2219, isolated probes ran the exact final APK's arm64 library with MS Gothic and Pretendard separately. Both pass native choice restore; normal, hover, pressed, cancelled, mismatched, and outside touches; both real story branches; and voiced/unvoiced replay. Each run produced 14 captures. The probes use separate temporary save/config directories and do not launch the installed main Activity.

MS Gothic's six normal/hover/pressed choice captures are full-frame pixel-identical to the old APK. The remaining eight dialogue captures differ only inside the animated flower wait marker: 147 to 320 pixels per image. All pixels outside the inspected marker rectangles are identical, including glyphs, outlines, names, and positions. Root inspected the paired marker crops and final Pretendard dialogue/choice captures. Pretendard labels have natural spacing and sit inside their buttons; descenders and the final dialogue line are visible.

Evidence: [Android build](../build/dc2dm-pretendard-layout-20261108/build-final-android.log), [APK manifest](../build/dc2dm-pretendard-layout-20261108/apk-verification.json), [MS Gothic probe](../build/dc2dm-save-validation/83b7bb8e319b/result.log), [Pretendard probe](../build/dc2dm-save-validation/97717bcca013/result.log), [MS Gothic pixel comparison](../build/dc2dm-pretendard-layout-20261108/device-analysis-msgothic-markers.json), [marker comparison image](../build/dc2dm-pretendard-layout-20261108/device-analysis-msgothic-markers.png), and [final phone captures](../build/dc2dm-save-validation/97717bcca013/captures).

## Save and settings audit

The resumed session's before/after test hashes match for all **10 PC and 16 phone save files**. No live save was restored or replaced by a fixture.

The main library file hash changed between the broad session snapshots, from `57dd2a21f6ee0a13f02ce40a622b2732df52b285e1a8cfd505ef321836a9e360` to `1e6f3579ebcce61bbe7febc7c96b456538764246d2887d9e4b34fbf5dd632c10`. Its recorded modification time is 12:32:09. Android lifecycle logs show the main Activity started at 12:29:51 and resumed at 12:29:56, independently of the isolated probes. `GameLibrary.save()` uses Java `Properties.store()`, which writes a timestamp comment, so this hash difference alone does not establish a changed setting. Only the initial hash was captured; the exact semantic difference cannot be reconstructed. The current library still selects bundled Pretendard JP Bold at size 22 for DC2DM. Current settings are preserved, without restoring an older library.

Evidence: [resumed save audit](../build/dc2dm-pretendard-layout-20261108/resumed-save-audit.json), [main Activity lifecycle](../build/dc2dm-pretendard-layout-20261108/main-app-lifecycle.log), and [current library stat](../build/dc2dm-pretendard-layout-20261108/library-current-stat.txt).

The [verified debug APK](../build/dc2dm-pretendard-layout-20261108/Arisa32-v0.7.0-dc2dm-pretendard-debug.apk) was installed successfully with `adb install -r`. The installed APK hash exactly matches `6bc13e7e923daa175b11f0427b23d808aa510aa9beb82d35c1c449b227c09d58`. All 10 PC and 16 phone save hashes are identical immediately before and after installation. The current main library hash remains `1e6f3579ebcce61bbe7febc7c96b456538764246d2887d9e4b34fbf5dd632c10` on both sides of installation. The installation did not launch the main game. Evidence: [installation verification](../build/dc2dm-pretendard-layout-20261108/installation-verification.json) and [install log](../build/dc2dm-pretendard-layout-20261108/install.log). `git diff --check` passes. No commit or release was published.

## Scoped antislop Delivery Gate

Root performed this gate for the changed DC2DM textbox/name/choice rendering using the source diff, final build/test results, and the images linked above. This is not a new accessibility certification for every unchanged game screen. Session mode: `during`. Direction declared before implementation: preserve original DC2DM artwork and layout for players, **ENERGY 2 / RHYTHM 1 / MOTION 2**. The following PASS entries apply to this correction's scope.

### Hard Gate

- R-02 PASS: no new product copy or decorative em dash was introduced.
- R-03 PASS: measured long text/choice bounds and four ruby/base rows fit the 1280 x 720 game canvas; root checked final phone captures.
- R-17 PASS: reported counts and hashes come from linked logs; no product statistics were added.
- R-18 PASS: no testimonials or invented people were added.
- R-23 PASS: no new product artwork, navigation, or font asset was created; existing authorized assets are reused.
- R-24 PASS: no navigation links were added or changed.
- R-25 PASS: ordinary text retains white ink and a black outline, a specified 21:1 color contrast; root inspected readability in the final scene/choice captures. Authored special text colors are unchanged.
- R-26 PASS: desktop branch selection and phone normal/hover/pressed/cancel/outside-touch probes pass.
- R-27 PASS: no loading, empty, or error flow was added; font-load failure resets the new identity flag in the targeted test.
- R-28 PASS: no FAQ was added.
- R-32 PASS: input handlers and focus behavior are untouched by this rendering correction; no mouse-only control was introduced.
- R-33 PASS: production behavior is implemented directly in the C++ sources; evidence scripts do not rewrite production code.
- R-34 PASS: no theme toggle or theme path changed; existing native and fixture artwork were both exercised.
- R-35 PASS: both Android ABIs built, 33 desktop tests passed, and both native phone probes exercised the affected choice states and branches.
- R-36 PASS: no security, compliance, performance, or customer claims were added.
- R-37 PASS: the original DC2DM visual direction and explicit dials guided the correction.
- R-38 PASS: real game scenes are identified as runtime evidence; synthetic text is identified as test fixtures.

### Purpose Gate

- R-01 PASS: existing pink gradients remain part of the original game's identity; none were added.
- R-04 PASS: existing game icons are retained; no generic decorative icon was introduced.
- R-06 PASS: the user-selected Pretendard face uses its own Latin metrics to correct recorded spacing; MS Gothic remains unchanged.
- R-07 PASS: no decorative background pattern was added.
- R-08 PASS: no decorative arrows were added.
- R-09 PASS: no badge or redundant label was added.
- R-10 PASS: the existing translucent game windows remain unchanged; no glass effect was added.
- R-12 PASS: the existing glyph outline separates text from artwork; no component shadow was added.
- R-13 PASS: no glow was added.
- R-14 PASS: no feature cards were added; authored choice rows remain the interaction surface.
- R-19 PASS: reveal remains stable, and existing wait/hover/pressed cues retain their interaction purpose.
- R-22 PASS: no illustration was created or replaced.

### Liveliness

- Dials PASS: ENERGY 2 / RHYTHM 1 / MOTION 2 are explicit and retain the original game's visual behavior.
- Consistency PASS: final dialogue and choice captures use the same authored windows, typography treatment, and state artwork.
- Focal point PASS: the dialogue or available choices remain the readable foreground of each affected state.
- Whitespace PASS: measured advances, wrapping, centering, and line spacing give letters and rows room within their containers.
- Accent PASS: the original pink selection accent remains visible in hover and pressed captures.
- Identity PASS: the blossom motif and DC2DM window artwork are retained.
- Design Read PASS: original-game direction and dials were declared before the source changes.

### Craftsmanship and quality locks

- C-1 PASS: the metric, sizing, fitting, and spacing decisions each have a concrete reason listed above.
- C-2 PASS: affected choices select the expected branches in desktop and device probes.
- C-3 PASS: no template section or new screen was added.
- C-4 PASS: targeted Latin/Japanese, reveal, ruby, tall-text, long-choice, and input-state checks pass; control profiles are byte-identical.
- C-5 PASS: every validation claim is tied to logs or clearly scoped visual inspection.
- R-05 PASS: original predictable dialogue and choice composition matches RHYTHM 1.
- R-11 PASS: existing button silhouettes and radii are unchanged.
- R-15 PASS: choice text comes from game data or named test fixtures; no generic CTA was added.
- R-16 PASS: no marketing copy was added.
- R-20 PASS: original blossom artwork and DC2DM framing remain visible in final captures.
- R-21 PASS: no forced theme or theme requirement was introduced.
- R-29 PASS: the existing game's palette is unchanged.
- R-30 PASS: no unrelated product layout was copied.
- R-31 PASS: each changed visual decision has its purpose recorded in the implementation section.

The UI skill checklist also passes within this scope: the palette, accent, shapes, and motion retain the existing direction; no badges, status dots, dashboard metrics, feeds, forms, or loading screens were introduced; the layout serves reading and choosing; existing controls and focus handlers are preserved. Runtime state coverage and its limits are stated above.

## Coverage limits

This is targeted rendering and compatibility validation, not a full game playthrough. The video was scanned exhaustively but inspected visually through samples and detailed frames. Backlog rendering is outside this correction. The arm64 library ran on the connected phone; armeabi-v7a was built and packaged but not run on a separate 32-bit device. Desktop tests cover Japanese and Indonesian; the real Android choice save uses Indonesian. Existing font-picker and unrelated screen behavior are not re-certified by this scoped gate.

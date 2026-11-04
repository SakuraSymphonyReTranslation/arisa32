# COMU custom-font readability, 2026-11-04

The Sakuramusubi COMU renderer previously capped all fonts at 16px and
used equal-width Latin cells. The user's `default.ttf` is Pretendard JP
Bold, a proportional face. Small lowercase letters and unnecessary gaps
made its phone messages difficult to read.

The supplied screenshots show the issue in the Sakuramusubi COMU scene.
The font was copied read-only from the connected iQOO's
`/sdcard/Kirikiroid/Da Capo 5 Sweet Happiness/default.ttf`.
Its SHA256 is
`2386cac04bdbe169c8243e55eac43d59c5d634a3776acf3865c406c99d7c259e`.
The original game files, patch files, and font were not modified.

## Rendering behavior

- Unpadded prose uses the selected font's measured advances. Faces with
  different measured widths for narrow and wide Latin letters use up to
  22px and up to three word-wrapped lines in the existing bubble.
- The preferred size also checks the actual message's vertical glyph
  bounds. Ink must fit the bubble and remain separate at the 19px row
  pitch; this accounts for faces with unusually high punctuation or
  larger ascenders at the same nominal size.
- The supplied font fits all 505 Sakuramusubi messages at 18-22px;
  152 messages use three lines. Previously this corpus used
  13-16px. Case and
  message bytes are preserved; `see` stays `see`.
- Short ASCII messages without DEL separators, such as `How?` and
  `Understood`, use the same prose layout. This covers the corpus's
  21 short messages as well as its 484 DEL-separated bodies.
- MS Gothic retains the native fixed grid, up to 16px and two lines.
  Prose starts at x=552 and fits a 228px width beside the portrait.
- Standalone `0xA5`, leading/trailing/repeated spaces, and explicit line
  breaks retain the author's fixed layout, 16px size, and x=540 origin.
  This preserves manual spacing independently of language or font choice.
- Proportional text uses baselines 30 and 56 within its text buffer, or
  18, 37, and 56 for three lines. The buffer begins 14px below the row
  origin. This leaves space for descenders and their outline. The native
  fixed-grid baselines remain 30 and 58.

The phone, bubbles, portraits, row spacing, timeline, and save format are
retained. The correction affects COMU drawing; ordinary dialogue font
size settings are not changed.

Font outline bounds provide an independent size check: lowercase `s` and
`e` span approximately 7-9 raster rows at the previous 13-16px sizes and
11 rows at 21-22px. `build/comu-custom-font/glyph-bounds.json` records
these bounds computed from the supplied font. Final renders are the
visual evidence, since coverage and outlines also affect appearance.

## Validation

The release build completed successfully for arm64-v8a and armeabi-v7a.
Desktop corpus validation reports 484 DEL-separated bodies and 21 short
messages, with zero failures. `text_render_test` and `scene_test` also
passed (2/2). Logs are in `build/comu-custom-font/desktop-result.log` and
`build/comu-custom-font/regressions.log`.

The production arm64 library passed the same corpus and scene assertions
on the connected iQOO Z9x (Android 16 / API 36), with process exit code 0.
All 12 desktop/device phone-region comparisons match exactly. Both
manual-padding samples and all five MS Gothic baseline scenes also match
exactly, for 19 comparisons with zero differing color components.
`build/comu-custom-font/render-comparison.json` records each result.

The APK was installed using `adb install -r` on device `10DF1N05QN000H8`;
ADB returned `Success`. The artifact is
`build/comu-custom-font/Arisa32-v0.6.6-comu-custom-font.apk`.
Its SHA256 matches the release output and the APK used by the probe:
`0f9b794272b83c8afc2d00cc28095aec98872bf8d912fa2a614fb13e0d36dac1`.
The tested arm64 `libarisa32.so` SHA256 is
`a3dbf7c5837e1d2d133ee2b52908ed2d1cb3575cb2ef0ae357c7582bc294a26e`.
Installation evidence is in `build/comu-custom-font/install.log` and
`build/comu-custom-font/installed-package.log`.
Package inspection confirms `org.dc4engine` version 0.6.6 (code 13),
minSdk 21, targetSdk 36, arm64-v8a, and last update
`2026-11-04 23:45:41` on the device.

`comu_text_test` checks the real 505-message corpus with both MS Gothic
and the supplied custom font. It preserves every word and verifies every
line's measured width, font size, and line count. The test also exercises
the bundled proportional UI face, so a custom-font regression can be
detected without the user's font file.

Scene tests cover both COMU opcodes. They compare the actual renderer
against font rasterization, exercise narrow/wide letters, short lowercase
text, long messages, and fixed-layout padding. A larger offscreen buffer
checks that the full glyphs and outlines fit vertically, rather than
accepting clipped output as a passing render.

Desktop reproduction:

```powershell
build/desktop/comu_text_test.exe android/app/src/main/assets/fonts/MSGOTHIC.TTC build/comu-custom-font/desktop 'H:/Games/D.C.4 Da Capo 4 Plus Harmony/Advdata' build/comu-sakuramusubi/corpus.hex build/comu-custom-font/default.ttf
ctest --test-dir build/desktop -R '^(text_render_test|scene_test)$' --output-on-failure
```

`build/comu-custom-font/android_probe.py` links those tests against the
production library extracted from the final APK and runs them on the
physical phone. `verify_renders.py` compares the desktop/device phone
regions, the earlier MS Gothic scenes, and manual-spacing samples using
both fonts. `comparison.png` shows before/after custom-font rendering
alongside MS Gothic.

The native probe runs through `app_process` and supplies the Android Java
`windows-31j` decoder used by the app. Its startup check verifies that
CP932 bytes `81 F4` decode to U+266A. An earlier standalone probe omitted
the app's decoder hook and misdecoded the three corpus messages containing
that symbol; its failed result is retained as
`build/comu-custom-font/android/result-without-java-decoder.log`.
This was a probe setup error; the APK already initializes the Java decoder.
The successful Java-hosted probe records its exit status; the native
test's buffered stdout was absent from this run.

This is renderer verification, not a full interactive playthrough or a
claim of Sakuramusubi PC line-break parity. It verifies the supplied font
and corpus, not a minimum readable size for every possible font or
arbitrarily long future message. Old saves with incorrect sender IDs have
the limitation described in `comu-sakuramusubi-validation.md`.

## Scoped antislop delivery gate

The scope is COMU typography with the supplied custom font. The direction
comes from the user's screenshots and readability request, retaining the
game's phone, portraits, bubbles, and colors. ENERGY 1 / RHYTHM 1 / MOTION 1
continue to describe the regular message stack; no new motion is added.
This is not an accessibility audit of unrelated app screens.

- Hard Gate PASS: the 505-message desktop check verifies word preservation and line width; offscreen render checks verify complete glyphs and outlines. The inspected comparison uses original game assets and retains white glyphs with black outlines. This change adds no controls, navigation, fabricated content, or marketing claims.
- Purpose-Gate PASS: measured proportional advances remove artificial letter gaps, sizes up to 22px improve lowercase readability, and a third line makes room for long messages. Vertical glyph bounds protect ascenders, punctuation, descenders, and separation between rows.
- Liveliness PASS: the phone stays the focal point; portrait-and-bubble rows and blue edges retain the original motif and accent. The comparison sheet shows the same structural spacing and message rhythm.
- Craftsmanship and Quality Locks PASS: both COMU opcodes, sender aliases, save/restore, short messages, manual padding, the supplied font, and the bundled proportional face are covered. The two manual-padding samples and five MS Gothic scenes have zero differing phone-region color components against the previous implementation. Corpus, device, and playthrough limits are stated above.
- Code comments PASS: added comments explain the proportional-font row budget, glyph bounds, and ownership of manual padding; they describe compatibility constraints rather than narrating routine operations.

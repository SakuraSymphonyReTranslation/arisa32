# COMU spacing validation, 2026-11-04

Manual padding in COMU messages now controls the line break in D.C.4 and
D.C.4 Plus Harmony. The previous renderer removed leading spaces, estimated
character widths, wrapped at word boundaries, and merged overflow into the
second line. That changed the layout authored by the translation patch.

## Recording review

Sources supplied by the user:

- PC: `20261104-1255-36.8681439.mp4`, 1276 x 750, 30 fps, 928 decoded frames.
- Arisa32: `comuarisa32_spacelogicnotsameaspc1.mp4`, 1072 x 480,
  approximately 29.7793 fps, 1,030 decoded frames.

Every frame was decoded and measured for image changes. Visual inspection
covered overview contact sheets, every distinct COMU state, full-size sample
frames, and the frames immediately before/at/after each detected Arisa32
message transition. This is not a claim of manually inspecting every full
frame individually. Frame numbers below are zero-based; timestamps are
approximate frame index / frame rate.

| COMU group | Arisa32 frames | PC frames | Visible progression |
| --- | --- | --- | --- |
| First discussion | 0-321 | 0-193 | Starts with three messages, then adds four; five rows remain visible |
| Shirakawa announcement | 715-822 | 586-687 | One, two, then three messages |
| Suggested wording | 851-966 | 722-805 | One through five messages |

Arisa32 state starts: 0, 70, 127, 153, 192, 715, 725, 749, 851, 860, 888,
924, 947. PC state starts: 0, 30, 63, 84, 108, 586, 611, 636, 722, 737,
755, 772, 788. The same 13 states appear in both recordings at different
playback speeds. The erroneous spacing is stable between message changes,
rather than a transient animation frame.

Examples from the matching local `dc4_miu20190319c.mes`:

| Body (padding omitted here for readability) | PC break | Previous Arisa32 break |
| --- | --- | --- |
| Begitulah situasinya, ada ... ide? | Before `ide?` | Entire message on one line |
| Intinya kita harus ... terlihat bisa diandalkan. | Before `terlihat` | Before `bisa`, leaving a large gap before `terlihat` |
| Publikasikan daftar ... anggota kita dulu. | Before `anggota` | Before `kita`, leaving `anggota` on the first line |
| Jangan, kata-kata itu ... terlalu kasar. | Before `terlalu` | Before `kasar.`, leaving `terlalu` on the first line |

Extracted frames, per-frame CSV measurements, transition sheets, decoded MES
messages with raw byte values, and comparison images are under
`build/comu-video/`. No proprietary game files were modified.

## Native behavior and implementation

Both original Windows engines insert **one newline after 28 SJIS bytes**
when the sender-stripped message is longer than 28 bytes:

- D.C.4 `DC4.EXE`: COMU renderer `0x458CC0`, length check `0x458E09`.
- D.C.4 PH `DC4PHDL.exe`: renderer `0x458A60`, length check `0x458BA9`,
  byte-28 cut `0x458BC8`, suffix source `0x458BDF`, newline at `0x48B810`.
- PH constructor `0x4586E0` sets x=540, y=94 and row pitch=81;
  the `@y` tag at `0x48B618` selects 16px text.

`comu_line_break` applies this rule before decoding or space normalization,
so the patch's `0xA5` spaces retain their positions. ASCII spaces are 8px
cells in the 16px COMU font. There is no word wrapping and no recursive
28-byte wrapping of the remainder. The first baseline is fixed at y=124
for the first bubble, with 28px line pitch; its placement was checked
against the PC recording. COMU uses the textbox font (MS Gothic in the
Android DC4 profiles), rather than the proportional UI face.

Two defensive extensions are explicit: a mixed ASCII/SJIS boundary moves
before a two-byte character instead of splitting that character, and valid
UTF-8 patch text uses equivalent one/two-cell counts. Unmappable UTF-8
characters are not claimed to reproduce CP932 byte behavior. Text is clipped
to the two-line bubble area; arbitrary oversized or manually multiline
messages are not claimed to match native overflow behavior.

The fixed-layout COMU path remains 16px independently of the user's ordinary dialogue font-size
setting. The earlier change allowing MS Gothic dialogue sizes remains in
place. This patch does not change the COMU timeline, stamps or tap pacing.

The later Sakuramusubi extension is documented in
`comu-sakuramusubi-validation.md`. Unpadded DEL-separated prose can fit at
a smaller size; the fixed-layout behavior described here is retained.

## Verification

- `comu_text_test`: passed. Checks 12 padded two-line messages from the
  recording plus its three single-line messages, 28/29-byte boundaries,
  long unbroken text, SJIS character boundaries, halfwidth padding, explicit
  newlines, and UTF-8 cell handling.
- Scene tests cover both MES versions `0xAAB6` and `0xABB6`: visible 16px
  displacement for two leading spaces, fixed first baseline, 28px line
  pitch, font selection, clipping, sender removal, five-row scrolling,
  hide/show, clear, stamps and save/restore pixel equality.
- Existing `text_render_test` and `scene_test`: passed.
- Release APK built successfully for `arm64-v8a` and `armeabi-v7a`.
- Installed successfully with `adb install -r`, retaining application data.
  Artifact: `build/comu-video/Arisa32-v0.6.6-comu-spacing.apk`;
  SHA256 `8a61cf647da0eb09185e448e690da526844fa20ee02f39af39f5db2f18440528`.
- The same COMU test passed on the connected I2219, Android 16/API 36,
  linked against `libarisa32.so` extracted from that release APK.
- Android library SHA256:
  `5e67293090c30811d1ff3323805463a9f954ed432bef0674b2683a1ac509f9b2`.
- The Android probe rendered the three opening messages with the original
  COMUCHIP asset. Its entire phone/chat region is pixel-identical to the
  portable desktop renderer. The comparison with the PC recording confirms
  line placement; Windows font hinting and recording resampling still
  differ, so exact cross-platform glyph-pixel parity is not claimed.

Desktop reproduction:

```powershell
cmake --build build/desktop --target comu_text_test text_render_test scene_test
ctest --test-dir build/desktop -R '^(comu_text_test|text_render_test|scene_test)$' --output-on-failure
build/desktop/comu_text_test.exe android/app/src/main/assets/fonts/MSGOTHIC.TTC build/comu-video/probe 'H:/Games/D.C.4 Da Capo 4 Plus Harmony/Advdata'
```

`build/comu-video/android_probe.py` records the device compile/run commands,
library hash and the captured original-asset render. This probe exercises
the production native renderer; it is not a full interactive playthrough.

## Antislop delivery check

Scope: restore the supplied PC layout using existing game assets and actual
MES text. No new screen, interaction, branding or marketing copy was added.
Typography and whitespace are chosen to reproduce the native grid and the
patch author's padding. Existing colors, avatars, phone shape and motion
are retained; visual dials inherit the supplied game rather than introducing
a new design. Relevant checks passed: text stays inside its bubble, render
and build were exercised, message/state operations have regression coverage,
and the report distinguishes measured results from untested behavior.
Site navigation, forms, testimonials, themes and newly designed interaction
requirements do not apply to this rendering correction.

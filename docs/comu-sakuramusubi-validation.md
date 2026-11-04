# COMU Sakuramusubi compatibility, 2026-11-04

This extends the fixed-layout COMU restoration in `comu-text-validation.md`.
The subsequent custom-font readability correction is documented in
`comu-custom-font-validation.md`; it extends the initial two-line layout
below for proportional fonts while preserving the MS Gothic samples.
The supplied Sakuramusubi recording shows incorrect sender portraits, words
split at byte 28, and long second lines cut off at the bubble edge.

## Evidence

`comuarisa32_spacelogicnotsameaspc2eng.mp4` contains 1,429 frames at
30.1799746 fps (about 47.35 seconds). All frames were decoded and measured;
414 contain the phone. Fifteen distinct phone states were identified.
Visual review covered those states, adjacent transition frames, and the
one-second overview sheets. This is not a claim that every duplicate full
frame was separately inspected by eye.

| Frame / time | Observed problem |
| --- | --- |
| 0 / 0.000 s | Ichito and Sorane use Nino's portrait; `ideas?` breaks inside the word |
| 127 / 4.208 s | Kanata's long message exceeds the second line |
| 704 / 23.327 s | The matchmaking-service message loses its ending |
| 795-846 / 26.342-28.032 s | Nino and Suginami messages have word splits and wrong portraits |
| 1336-1341 / 44.268-44.433 s | Nino's long message is cut off; Ichito again uses Nino's portrait |

The phone's active `Advdata/MES/dc4_miu20190319c.mes` matches the copy in
`eng_data/MES`: SHA256
`72202cab936edcd66626a78506e70ed01abfce00c101b80f13cda2f466bde7dc`.
Its 17 COMU messages match the recording. Scanning 495 MES files found 505
sender-prefixed COMU messages: 484 use DEL (`0x7F`) word separators, and
21 are short messages without spaces. The longest body is 69 bytes.

Sakuramusubi's `DC4PHDL.exe` contains an English sender table at VA
`0x4ADABC`: Arisu, Nino, Sorane, Hiyori, Shiina, Miu, Chiyoko, Arisa,
Ichito, Suginami, Kanata. Arisa32 previously matched only the Japanese
table and fell back to Nino for every translated sender.

The supplied Sakuramusubi EXE still contains the original byte-28 COMU
split at `0x458BA9`. Therefore word-aware fitting here is an Arisa32
compatibility improvement, not a verified reproduction of Sakuramusubi's
PC layout. No Sakuramusubi PC recording has been supplied for pixel or
line-break parity. The Android recording alone cannot establish that.

The user's other patch contains `DC4PHPatch.dll` and `DC4PHLauncher.exe`.
Its DLL exposes dialogue/backlog font settings and its INI configures
MS Gothic. These files demonstrate a different patch architecture;
DLL presence alone does not establish a COMU line-breaking rule. Nothing
from either patch is executed or modified by this investigation.

## Behavior

- Both DC4 profiles accept the eleven romanized sender names alongside
  their Japanese names. Unknown-name fallback remains Nino.
- Unpadded DEL-separated message bodies use word boundaries and the
  largest font size up to 16px that fits two 228px lines. This path starts
  at x=552 so unpadded text clears the sender portrait. The padded path
  retains x=540. Both native baselines remain.
- Standalone `0xA5`, leading/trailing/repeated spaces, and explicit line
  breaks retain the existing byte-28/manual-layout path. A byte inside a
  multibyte SJIS character is not mistaken for a padding marker.
- Detection depends on raw message formatting, not language, folder name,
  or the presence of a launcher. An English version of the user's padded
  patch retains its manual spacing. Short Sakuramusubi messages without
  spaces already fit the original path.
- Original message bytes remain in COMU state. Save format, timeline,
  stamps, ordinary dialogue font-size settings, and the game files are
  unchanged by this extension.

## Validation

Final desktop and physical-device checks passed for all 505 messages:
484 DEL-separated bodies and 21 short legacy bodies, with zero COMU
failures. Existing `text_render_test` and `scene_test` also passed. The
release APK built successfully for both arm64-v8a and armeabi-v7a; the
physical-device probe exercised arm64-v8a on the iQOO I2219, Android 16.
This does not establish runtime coverage of every Android version or ABI.

All six desktop/production-Android renders have zero differing pixels
within the compared phone region. The Indonesian sample also has zero
differing phone-region pixels against the previous APK. Visual inspection
of `verified-renders.png` confirms intact words, text clear of portraits,
and complete long messages within the two-line bubbles. The corpus uses
font sizes from 13px through 16px.

Evidence is saved locally in `build/comu-sakuramusubi/`:

- `desktop-result.log`: 505 messages, zero failures.
- `android/result.log`: the same checks against the library from the final APK.
- `render-comparison.json`: six render comparisons and the prior-APK comparison.
- `verified-renders.png`: the inspected comparison sheet.
- `installation.txt`: successful replacement installation and package metadata.

The final APK was installed with `adb install -r` on device
`10DF1N05QN000H8`; ADB returned `Success`. Package inspection confirmed
`org.dc4engine`, version 0.6.6/code 13, minSdk 21, targetSdk 36, and
`lastUpdateTime=2026-11-04 22:21:45`. The distributable is
`build/comu-sakuramusubi/Arisa32-v0.6.6-comu-sakuramusubi.apk`.

| Artifact | SHA256 |
| --- | --- |
| Final APK | `9d9111a25d9e6fff85f4a259d5c26df0d8dc2e9594230774d21828cbebde3e15` |
| Extracted arm64-v8a libarisa32.so | `7fccba08b2f14dd34d6df5d0aa2a32370482fdb95a95e6e839df72586f278938` |

Proprietary MES and executable files used during investigation stay in
the local build directory and are not added to the repository.

The test covers all 17 messages from the supplied scene, all eleven
sender aliases for both `0xAAB6` and `0xABB6`, language/encoding changes,
and save/restore with a long COMU message visible. A raster comparison
checks that the chosen smaller font actually reaches the renderer. The
corpus check validates all 484 DEL-separated messages without dropping
words, and verifies that the other 21 messages fit the legacy path.

Desktop reproduction:

```powershell
build/desktop/comu_text_test.exe android/app/src/main/assets/fonts/MSGOTHIC.TTC build/comu-sakuramusubi/desktop 'H:/Games/D.C.4 Da Capo 4 Plus Harmony/Advdata' build/comu-sakuramusubi/corpus.hex
ctest --test-dir build/desktop -R '^(text_render_test|scene_test)$' --output-on-failure
```

`build/comu-sakuramusubi/android_probe.py` links the same tests against
the production library extracted from the APK, runs them on the physical
phone, and captures six renders with the original COMUCHIP. The comparison
script checks every pixel in the phone region against the desktop output
and compares the Indonesian sample against the previous installed APK.
This is a native renderer probe, not a full interactive playthrough.

The UTF-8 comparison checks the phone region. The standalone Android
probe uses the core's limited CP932 fallback rather than the app's Java
decoder; unrelated Japanese UI outside COMU can differ when the encoding
changes. Romanized sender matching happens before decoding.

An old Arisa32 save captured while COMU was visible can retain the old
incorrect contact IDs. Save entries contain a contact number and the body
with its sender name already removed, so the original sender cannot always
be recovered. Replaying that COMU group rebuilds its portraits correctly;
new saves preserve the corrected IDs. Text fitting applies to restored
message bodies immediately.

## Scoped antislop delivery gate

This gate covers the COMU rendering change and its validation report.
The requested direction is restoration using the game's existing phone,
portraits, bubbles, colors, and MS Gothic. It is not a new interface design
or an audit of unrelated app screens, accessibility, or navigation.
For this static COMU layout, ENERGY 1 / RHYTHM 1 / MOTION 1 describe the
retained regular message stack; existing game transitions remain outside
the change.

- Hard Gate PASS: the 505-message checks and six inspected renders verify text fitting and sender mapping; original assets and text colors are reused, no controls or navigation are introduced, and PC parity and device-coverage limits are stated above.
- Purpose-Gate PASS: the word-boundary split preserves readable words, x=552 clears the portrait, the 228px measure keeps text in its bubble, and using the largest fitting font preserves readability; the padded path retains its authored coordinates and spacing.
- Liveliness PASS: the original phone remains the focal point, repeated portrait-and-bubble rows retain the game's motif and regular rhythm, blue bubble edges remain the accent, and the comparison sheet verifies structural spacing without new decoration or motion.
- Craftsmanship and Quality Locks PASS: original message bytes survive save/restore, all eleven sender aliases are checked for both COMU opcodes, the prior Indonesian sample has zero differing phone pixels, and desktop plus production-library checks provide the recorded evidence for the claims.
- Code comments PASS: comments in the COMU helper explain padding ownership, the renderer measurement constraint, and the native split address; they document compatibility constraints rather than narrating routine operations.

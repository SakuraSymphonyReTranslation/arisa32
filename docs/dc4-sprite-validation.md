# DC4 / DC4PH sprite corrections (2026-11-02)

## Missing pocket-watch cut-in

`dc4_kyo20190204a2.mes` loads `BST\CUT007` into slot 25, then executes
`40(0,".")` and `40(1,"-n")` before zero-based dialogue line 63. It remains
visible through line 66 (Indonesian: "Lalu, aku berkonsentrasi.") and is
dismissed by `8C(0,3,0,128)` before line 67.

Arisa32 treated both `-` and `-n` as removal commands. Native DC4 at
`0x42CCE0` and DC4PH at `0x42CB50` instead store the numeric operand for the
effect object's draw flag. DC4 applies it at `0x43159E` through `0x444240`,
which writes object offset `0xB8`; drawing at `0x444D3D` checks that flag.
Thus `40(1,"-n")` enables drawing and `40(0,"-n")` disables it. The operand
is not an effect-layer index. Native `0x40` targets slot 25, including when
another effect slot was loaded more recently.

The correction is restricted to detected DC4/DC4PH games with MES versions
`0xAAB6` / `0xABB6`. Explicit removal, cut-in fade clocks, sound dispatch,
other profiles, and the existing `0x8C` effect selector remain unchanged.
It does not depend on translated dialogue or byte offsets.

Corpus scan: all 511 DC4 and 536 DC4PH MES files parsed. `0x40 -n` occurs
44 times across 31 DC4 scripts and 48 times across 32 DC4PH scripts. These
counts include intentional hiding and initial setup; they are not counts
of confirmed broken scenes. References include CUT007, CUT045 and the
PH-only CUT049A/B/C sequence.

## Nino's two zoom frames

`dc4_sen20181213d.mes`, lines 140/141, loads loose CRX `BST\CUT025` after
`8B(65,0,0,16)`. Native DC4 `0x443616` / DC4PH `0x4433F6` resolve a zero
zoom anchor to half the source rectangle's size on each axis independently.
Arisa32 previously used literal zero and displaced the image when scaled.

Only the verified loose-CRX path in DC4/DC4PH now resolves these implicit
anchors before applying existing camera math. CRM placement and nonzero
anchors retain their previous calculations. Raw anchors remain unchanged
in saved state. No dialogue matching or translation-specific offsets are used.

## Verification

- `scene_test`: implicit anchors on each axis, explicit anchors, 50%/100%
  sprite scale, camera scaling, and save/load; both MES versions.
- `scene_test`: cut-in enable/disable/restore, fixed slot-25 targeting,
  explicit removal, reuse after fade, and save/load; both MES versions.
  The previous implementation fails the visibility tests.
- Dismissal remains partly visible after 150 ms and hidden after 300 ms
  for `8C(0,3,0,128)`; skip/finish settles it immediately.
- Desktop CTest: 23 tests passed in the full run; the scene test's assertion
  was corrected to check invisibility after fade (the engine resets alpha
  when hidden), rebuilt, and passed on rerun with the same game-data environment.
- Actual DC4 and Indonesian DC4PH captures: watch absent at line 62,
  present at 63/66, absent at 67. Additional captures cover CUT045's toggle
  and PH CUT049A/B/C at lines 124/132/147 of `dc4_asa20190511b`.
- Nino lines 140/141 visually match the supplied PC framing. Eight control
  frames (lines 57/139/144/171 in both games) are byte-identical before/after.
- Android debug build succeeds for arm64-v8a and armeabi-v7a. The focused
  `scene_test --dc4-sprites` suite passes on the connected Android phone,
  linked to the arm64 library extracted from the delivered APK. This is a
  native-library probe, not a full interactive playthrough.

Local evidence: `build/nino-validation/` contains before/after captures,
native disassembly, corpus inventory, build/test logs, and the APK.

APK: `Arisa32-v0.6.3-dc4-sprite-fix-debug.apk` (version 0.6.3, code 10).

SHA256: `513d04fdbc7a98f05c4cd684353ecca79c3881aa1cc9eea73e39d53af531d826`.

Tested arm64 library SHA256:
`3c283016e8397f3aa8c92efe2c4f498511cbc4a77465da74449b133cfc18388f`.

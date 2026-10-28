# Menu, sprite and DC2PC replay validation — 27 October 2026

This build fixes the reported menu announcements, DC2PC sprite cropping,
DC4/Plus Harmony music catalogue indexing, and DC2PC replay Save/Load behavior.
It includes the earlier FFmpeg work documented in `ffmpeg-validation.md`.
The APK hashes below supersede that report's earlier delivery hashes.

## Changes

- Menu entry announcements use a shared page-to-event mapping in Android and
  desktop, including Android Back transitions. Extra uses modern event 010 or
  DC2PC event 05; DC2PC Config uses 10 and Interview uses 09. Gallery remains
  015/06, Music 016/07 and Scene Replay 017/08.
- Modern catalogue row zero reads the unlock flag and audio asset for BGM 001;
  BGM 000 is the dummy slot. The script supplies the catalogue length: 42 rows
  for DC4 and 43 for Plus Harmony. Playback, favorites, random selection and
  scrolling use the same count. Continuous scrollbar dragging survives layout
  reconstruction. Existing progress files do not need migration.
- RGBA CRXD composition retains the union of base and patch bounds, with the
  original registration offsets. Expanded transparent pixels retain the most
  common transparent RGB from the base (black if unavailable); this matters
  because additive differences encode RGB even in transparent areas. DC2PC's
  magenta background must be preserved to avoid cyan/green restored edges.
  The RGB/indexed composition path is unchanged.
- DC2PC replay uses the authored `kaiso_menu` sidebar: WIN/FULL, CONFIG, TITLE,
  CLOSE. Native replay flag `9[7] == 1` disables Save/Load, quick actions and
  persistent pause/autosave paths in core, Android and desktop. Internal
  snapshots remain available for rendering/history. Leaving replay clears the
  replay flags; ordinary story Save/Load is available again.
- Pausing replay clears retained-session metadata, so an older story checkpoint
  is not mistaken for the paused replay. The checkpoint file itself is preserved.
  Modern DC4/PH replay persistence behavior is unchanged.

## Validation

| Check | Result |
| --- | --- |
| Desktop build and final CTest run | Passed, 17/17 targets |
| Android `assembleDebug assembleRelease` | Passed for arm64-v8a and armeabi-v7a |
| APK signatures | Both passed `apksigner verify` |
| Release APK alignment | Passed `zipalign -c -P 16 4` |
| APK native payloads | Both ABIs match the corresponding final stripped build libraries |
| Actual DC4 catalogue | Passed: 42 rows, first/last playback, unlocks, favorites, random and scrolling |
| Actual Plus Harmony catalogue | Passed: 43 rows, including the final Harmony title |
| Actual DC2PC menu suite | Passed: sidebar controls, replay save rejection, preserved sentinel save, story restoration, music, Interview, config and progress |
| DC2PC Extra/Interview announcement assets | All 16 selected voices resolve and decode to non-silent PCM for both events |
| DC4/PH Extra announcement assets | SYSTEM_ASA010 found in both original VOICE.DAT archives |
| Actual DC2PC flow suite | Passed: ordinary story continuation, replay ending and catalogue return, four route branches |
| Actual DC4 menu/replay suite | Passed: 469 scene selections and 48,639 replay dialogue commands |
| Actual DC2PC CRM archives | Passed: 4,118 variants in 99 archives |
| Synthetic CRM cases | Passed: alpha conventions, linked differences, expanded/disjoint/empty bounds, background RGB, rejected oversized bounds |

The original asset directories were read only. Test save writes used isolated
temporary fixtures. Two old test setup assumptions were corrected: the modern
locked-Music case explicitly sets its feature flag after Reset Settings, and
the DC2PC flow test reopens the catalogue after simulating newly unlocked scenes,
matching the host sequence.

Visual checks used the actual Otome front/rear and Yume rear variants from the
reported poses, plus rendered replay sidebar and music catalogue captures.
Otome OTM_0B_000 now has bounds `(265,44), 398 x 712`; the old base crop was
`(273,83), 303 x 673`. The restored ribbon pixel at game coordinate `(590,297)`
is RGBA `(255,237,236,255)`.

Local captures are under `build/menu-regression/`:

- `sprites-verified.png` composites the three decoded variants on a neutral
  background for visual inspection.
- `dc2pc/sidebar-replay.png` shows the four-row replay sidebar.
- `dc4/` and `dc4ph/` contain first/last music-page captures. These test fixtures
  deliberately unlock only the first two and last tracks; other blank rows are
  intentional locked entries.

## Delivery and device limits

The release APK was installed with `adb install -r` on the I2219 ARM64 device
without clearing app data; package update time is 2026-10-27 15:17:35 local.
The phone remained in another foreground application, so no controlled app UI,
speaker listening, or Android process-recreation test was performed after this
installation. Android lifecycle guards were checked by source review and build;
the replay save/restore rules and visual checks ran in the shared desktop core.
The 32-bit ABI was built and packaged but not run on a 32-bit device.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `android/Arisa32-debug.apk` (also `android/dc4-debug.apk`) | 52,194,142 | `1bb92958848044eb97acc0e27e92ee6685078a22a6754c196b25d9857bae42f6` |
| `android/app/build/outputs/apk/release/app-release.apk` | 13,120,903 | `6cb8ca6102c134021a50d4b8b73fa94d4c88af41e715bd8a595149bd0f2a4975` |

The release build retains the project's existing debug signing configuration.
AGP/compileSdk, deprecated API and Java native-access warnings are unchanged;
they did not prevent either build or signature verification.

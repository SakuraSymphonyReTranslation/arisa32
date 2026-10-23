# Opcode profiles and configuration

Opcode service names live in `core/include/arisa32/mesopcode.h` as
`enum class Opcode : uint8_t`. `MesCommand::op` retains the encoded byte for
disassembly, tracing and save fingerprints. Execution adapts verified DC2PC
commands through `mes_engine_command` before using named engine services.
Operand kind remains part of dispatch because older dialects reuse byte values
for different operations.

## Data that belongs in profiles

| Setting | Location | Purpose |
| --- | --- | --- |
| Header length and operand ranges | `core/src/mesopcode.cpp` | Decode each MES dialect with one bounds-checked walker |
| Verified opcode aliases and operand transforms | `core/src/mesopcode.cpp` | Translate DC2PC services without executing unrelated DC4 commands |
| Authored canvas, menu entry labels, progress range | `MenuProfile` in `core/src/system_menu.cpp` | Keep title, Extra and config layout/progression specific to the game |
| Game detection, save namespace, resource search paths | Java `GameProfile` implementations | Preserve existing game profiles and select DC2PC independently |
| Volume, text speed, skip/auto and effect preferences | `SystemConfig` and persisted menu variables | Store player preferences using the existing profile format |

Keep VM control flow, rendering, timing, lifecycle and save validation in code.
Do not configure arbitrary executable handlers or broad version aliases. New
aliases need evidence from that game's scripts or original interpreter.
Unrecovered DC2PC commands resolve to `Opcode::UNKNOWN`.

## DC2PC boundaries

DC2PC uses the ClassicDC2 dialect with version `0x9969`, four-byte label entries
and a two-byte code header. The authored menu is 800×600, centered inside the
shared 1280×720 frame; touch coordinates use the inverse transform. Modern
menus retain their 1280×720 canvas and existing entry labels.

DC2PC title selection runs the original `main.mes` entry. Bank 100 progress
indices `[0,23)` synchronize between the story VM and menu. Flags 1 and 2
unlock da capo, flag 7 unlocks al fine, and flags 1–17 let the original script
set flag 21 for Interview. The title's derived flag 57 stays outside that sync
range so the script can compute it.

Android and desktop use `AdvData/arisa32-saves/dc2pc` for emulator-owned DC2PC
saves. The desktop `DC4_SAVE_DIR` override still takes precedence. Existing
DC4, Plus Harmony and generic profile selection remains ordered and keeps its
previous save behavior. Original game save records are not converted by this
change; unlock tests use VM progress and emulator-owned profiles.

## Verification on 2026-10-23

- Exhaustive decoding of all 256 bytes for Modern, ClassicDC2, ClassicDM and
  DC2PC, including every truncated command prefix, passes.
- Installed DC2PC title, partial/full progress, Interview action, config and
  emulator profile round trip pass. Java profile detection/isolation passes
  21 checks.
- DC4 corpus results match an isolated build of Git HEAD: 469 chapters,
  48,059 dialogue commands, 26 choices and 469 transfers. Both builds report
  the same six real-asset VM assertions. Scene Mode checks cover 469 choices
  and 48,639 replay dialogue commands; both builds retain the existing
  `!has(m,"musmode")` assertion failure.
- Default CTest retains three baseline failures: opening-movie overlay
  selection, label case lookup and sprite reset after preserved dialogue.
  These were reproduced in the isolated HEAD build.
- Plus Harmony title rendering passes; the DC4-specific 469-entry replay
  harness is not a complete Plus Harmony regression suite.
- The older Java archive harness currently fails its strict game-identity
  expectation because the existing importer skips the manifest. It is not
  counted as a passing check.

### DC2PC translated text

The local `DC2PCLauncher.exe` loads `DC2PCPatch.dll`. The patch source's
`Hook_Hsxf4_1C970` converts fullwidth Latin Shift-JIS to ASCII and adjusts the
glyph pitch/cursor advance; `SelectHsxf4LatinFont` selects the configured
MS PGothic face. Disassembly of `DC2PC.EXE` at `0x41C970` confirms the pitch
global at `0xB84F20` and cursor at `0xBD590C`. Translated opening MES dialogue
contains fullwidth Latin, explaining the spaced letters and intra-word wraps.

Latin display text now normalizes those letters and punctuation after ruby
syntax parsing. Wrapping, styled runs and ruby centering share FontRenderer
advance metrics. DC2PC Latin uses the bundled PGothic face (TTC index 2),
18px text and baseline 480 on the authored canvas, visually checked against
the supplied Windows screenshot. Japanese glyph cells remain unchanged.
This matches compact typography rather than reproducing every DLL glyph hook.

The portable stb regression passes fullwidth/ASCII equivalence, proportional
width, word wrapping, colour-run placement and partial reveal. Installed
Japanese and Indonesian DC2PC menu/opening checks pass. Desktop and Android
builds pass; the updated APK installs successfully on the connected device.
The device loads the bundled font and displays compact Indonesian dialogue;
`build/dc2pc-check/device-text.png` records the verified opening scene.
CTest passes 10/13 with the same three baseline failures listed above.

This refactor covers shared MES decoding, VM operations, scene/menu dispatch
and profile selection. Other host UI and lifecycle paths still need separate,
behavior-focused refactoring; removing every conditional would obscure their
state transitions.

### DC2PC scenario flow and replay

The premature opening return was caused by treating every menu `StartScene`
action as a replay in both hosts. DC2PC's authored title `start` transfer now
returns `NewGame`; only a transfer selected from Scene Replay returns
`StartScene`. The opening tests follow the real menu action through the host's
replay predicate. Ordinary play leaves bank 9[7] at zero, suspends for raw 0x38
(`movie/dc2fst.mpg`), then transfers to `fst_1216_b1_cmn`. Replay sets 9[7] to
one and returns through `sc_end`, which the hosts intercept as the replay menu.
An expected menu transfer no longer produces the Android end-of-scenario toast.

Story order comes from MES transfers and comparisons, not catalogue order.
For example the tail of `fst_1217_a1_cmn` selects `fst_1217_a2_cm1`, `cm2`,
`cm3`, or `cm4` from flags 7[0], 7[1], and 7[40], falling back to cm4.
Installed-data tests execute each of those four paths. Player choice/minigame
UI beyond this recovered flow has not been exhaustively verified.

Native variable functions at 0x41B180/0x41B430 implement 100-byte ordinary banks
0..9, 100 bytes for bank 100, and 1500 bytes for each of banks 97 and 98.
Banks 200..209 expose overlapping big-endian 32-bit values in banks 0..9;
ordinary byte assignments clamp to 0..255. Both VM and menu use the recovered
alias for replay list scroll, 206[80], corresponding to bytes 6[80..83].
Host music flags in bank 96 remain a portable extension; bank 99 services are
only partially recovered.

The version 6 catalogue has 1336 CG records and 1434 scenario records. Opening
is scenario index 1036. In `sce_other`, the unlocked item with global flag
98[1036] carries local action tag `sce0`; these IDs are not interchangeable.
Replay rows now select items by ordinal position while retaining their global
unlock keys. Return restores character 3[3] and scroll 206[80]. Authored gallery
thumbnail targets group CG variants; cycling stays within the selected group
and excludes unseen variants.

The progress observer normalizes DC2PC commands before classifying them:
raw dialogue 0x4B..0x4D marks the current scenario, raw background 0x30 marks
matching CG catalogue assets, and raw BGM 0x05 marks music. Emulator profiles
store asset names and reconstruct flags from the catalogue on load. This is
host unlock timing; native scenario unlock timing is not yet fully recovered.
The installed-data regression checks opening continuation, replay return,
local/global ID mapping, scroll restoration, four route transfers, high CG and
scenario flags, alias snapshot round trips, CG variants, and profile persistence.

### DC2PC System Config sliders

The white rectangle over the text-speed row was a renderer error. Raw 0x50
followed by raw 0x52 type 24 is a discrete selection strip, not a movable
thumb. Treating its operand 752 as the sprite width drew unused white atlas
pixels into the adjacent controls and pillarbox. Native DC2PC helper
0x412E70 stores types 20/21 as slider metadata; 0x413590 uses type 21's
explicit interval stops, clips type 24 to the strip, and clips types 25/26
to the selected interval. Types 22/23 respectively fill a continuous track
and move its thumb. The menu renderer and touch handler now implement these
separate behaviors using the authored coordinates (text speed: 527, 557,
586, 631, 679). Fast and Auto Fast select immediate reveal, confirmed at
0x421D60; ordinary display-speed control remains separate.

Installed-data regression covers all four text-speed selections, continuous
slider endpoints, setting persistence, and the previously overwritten black
pillarbox. Rendered captures cover System Config and voice settings. Replay
list captions consume raw 0x58's following string and fit within their cells;
rebuilding the list removes old captions along with the old controls.

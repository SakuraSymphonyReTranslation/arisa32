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
| Game detection, save directory, resource search paths | Java `GameProfile` implementations | Preserve existing game profiles and select DC2PC independently |
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

Android and desktop reuse existing game save folders, including `SaveData`,
`Savedata`, `savedata` and other case variants. When no folder exists, DC2PC
creates `AdvData/SaveData`. DC2PC, DC4 and Plus Harmony writes use native `.dat`
filenames and numbering. The quick files are `DC2PC00.dat`, `DC402.dat` and
`DC4PHDL02.dat`; ordinary slots start at 02, 03 and 03 respectively. Legacy
`slotN.dc4sav` and `quicksave.dc4sav` remain read fallbacks. Native files win
when both formats occupy the same slot. The desktop `DC4_SAVE_DIR` override
still takes precedence; generic profiles retain emulator snapshots.
On Android, snapshots left by older APKs in `AdvData/arisa32-saves/dc2pc`
are copied to the selected save folder without overwriting existing files.

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
raw dialogue 0x4C marks the current scenario, raw background 0x30 marks
matching CG catalogue assets, and raw BGM 0x05 marks music. Emulator profiles
store asset names and reconstruct flags from the catalogue on load. This is
host unlock timing; native scenario unlock timing is not yet fully recovered.
The installed-data regression checks opening continuation, replay return,
local/global ID mapping, scroll restoration, four route transfers, high CG and
scenario flags, alias snapshot round trips, CG variants, and profile persistence.

### DC2PC nameplate, dialogue history and sidebar

Installed Japanese and Indonesian scripts use raw `0x4B` for the speaker and
`0x4C` for dialogue; `0x4D` supplies a choice caption. A speaker command and its following dialogue are
consumed together. The floral message frame is drawn in full for narration
as well as named dialogue, because the flowers cross the nameplate/body
boundary. Speaker text is centered separately and shrinks to fit the plate.

The original textbox Up/Down buttons browse up to 4,000 previous lines while
retaining the live VM, flags and save position. Down stops at the current
line. A plain tap returns to the current line without advancing. History
survives story transfers, resets after loading a save or scenario, and pauses
auto/skip while it is displayed.

DC2PC's Arisa32 settings offer Show/Hide Sidebar. The drawer uses the original
`advmenu.mes` controls and `quickoption_chip00.crx` artwork, with WIN/FULL,
Save, Load, Config, Title and Close routed to the host. Title retains its
confirmation. Desktop also supports Tab to toggle the drawer and Up/Down to
browse dialogue. The drawer pauses story progression and is never serialized
into a save.

### DC2PC choices, sprites, voice and saves

Choices use the original panel fill at alpha 128, the selection flower and
cyan border at full opacity (including touch-down), and authored
choice labels at 28px before fitting long captions into the native 480px row.
Classic BST references such as `bst\\sgn_0a_000` resolve to the shared
`sgn_0a.CRM` archive, without requiring a delimiter in the script. `_b`
suffixes remain real variant names; they are removed only when deriving the
archive name. Loose CRX sprites retain priority. Character
placement uses the DC2PC canvas. Raw `0x30` background changes dismiss body
layers 1..4, matching native 0x425884; between changes bodies persist until
raw `0x2B` hides or replaces them. Raw `0x64` selects the sprite
through word 2, sets its signed position and resets scale; raw `0x65` modes
0..4 set positions, 16..19 set scale and anchor, and 255 changes the first
two scales. Raw `0x64` index 255 swaps bodies while retaining each layer's
transform. Translation retains the raw opcode to distinguish these paths.
The current renderer applies position changes immediately; native stepped
movement animation is not yet reproduced.

Compression-3 registration metadata supplies the full sprite canvas rather
than its cropped image bounds: ordinary bodies use 882x756 and close-ups
1260x1080. Native helper 0x41C000 adjusts the close-up anchor at scale 100.
DC2PC backgrounds fit the original 800x600 viewport. Sprite and UI layers
are clipped to that viewport so they cannot spill into the black margins.
Lighting marker `@`
multiplies RGB by 3/4 and adds 16 to blue; `*` multiplies RGB by 7/8.
Raw `0x2C` restores the clean body and composites a partial expression at its
registered offset, adapting the `_b` suffix to the body canvas. Save format
v5 persists expression references; versions 1..4 remain readable.

CRM expression patches add RGB deltas and subtract decoded opacity modulo
256. The CRXG mode selects each block's alpha convention: DC2PC uses inverted
alpha in its bases (mode 0), direct alpha in its patches (mode 2), so raw
alpha is added rather than subtracted. The decoder applies this to all CRM
characters, including linked CRXJ patches. Synthetic regressions exercise
all base/diff mode combinations and clipping; `DC2PC_CRM_DATA` enables a
complete installed BST archive sweep.

DC2PC `VOICE.DAT` uses its own index layout. Scenario and menu voice requests
resolve through that index and decode XPCM audio. Scene Replay captions
include the scenario's authored date.

The save/load picker uses the original backgrounds, Quick and Latest rows,
and three numbered slots on each of ten pages. Save selection opens the
original 16-character icon chooser, then confirmation when enabled. Cancel
does not write a save. A successful save persists its selected icon in a
sidecar; desktop sidecars carry the save fingerprint to reject stale icons.
Quick Save stays in the picker, while Load resumes the selected scene.
Quick, Latest and numbered rows use the green, blue and cyan atlas rows.
Row captions and timestamps are white, matching the PC picker.

Android's DC2PC-only Back / Sidemenu button sits below the three-dot control
in the left black margin, clear of display cutouts. Its label uses exactly
two lines, `Back /` and `Sidemenu`. It cancels confirmation or icon selection first,
returns from Save/Load and native menus, and toggles the sidebar during a
scene. Hardware Back shares this route. The same button closes the sidebar.
The drawer artwork and hit rectangles share the scenario's centred 4:3
origin, retaining the authored positions without a second margin offset.

Validation includes synthetic multi-character persistence, signed positions,
scale reset, lighting, partial expression replacement and Save/Load, plus
installed b1/b11/b2/c1 scenarios for left/right placement, the three-character
scene, Maya's close-up, room changes, and retained Suginami. Render
captures compare choices and save rows with the supplied PC screenshots.

### DC2PC PC save import

DC2PC / Plus Communication is a specific Da Capo II variant. Its profile
uses identifier `dc2pc`; the base game, Spring Celebration, To You, Fall in
Love and Dearest Marriage are not selected by the family name alone. Speaker
names in synthetic tests are sample dialogue, never game identifiers or a
character whitelist. Sprite assets and speakers come from each scenario/save.

Native `system.dat` has no embedded title string. Import therefore requires
the detected DC2PC profile and a validated native structure, rather than a
filename or exact length alone. The known native block is at least 0xCF4:
global bank 100 at 0x74 (100 bytes), CG at 0xD8 (1500), scenario flags at
0x6B4 (1500), and BGM flags at 0xC90 (100). Trailing extensions are allowed;
truncation and invalid catalogue flags are rejected before mutation. Imported
clear flags merge with Android progress and survive an older empty profile.
Native title scripts determine bonus-story unlocks from those clear flags.

Android searches game roots and `SaveData` case-insensitively. Numbered PC
regular files `DC2PC02.dat` onward and Quick `DC2PC00.dat` take priority over legacy
Android snapshots. Loading a slot does not write PC saves or `system.dat`.
The merged achievements persist in `system-profile.dat` in the emulator
namespace. Native slot script names start at 4, backgrounds at 0x84, five
body records at 0x104, BGM at 0x804 and local byte banks at 0xFF0. Hidden
records have a zero first byte, even when stale asset bytes remain behind it.
Physical 01 is the PC latest-save copy, excluded from regular write targets.
The Latest row reads this copy when newer than the numbered saves, including
when it is the only save available. It is preserved during regular saves.
Regular logical slots 1..3 map to physical 02..04; 4..8 map to 08..12, and
later groups stride by eight. The original menu exposes 48 regular slots
through physical 76. Extra emulator slots remain outside the PC menu range.

Version-1 PC slots use raw03 dialogue and raw04 choice checkpoint identifiers
to find the equivalent position across Japanese/Indonesian script lengths.
Restore reconstructs placement and reads saved assets/local variables without
executing previous story assignments or granting additional clear flags.

Native exports have fixed sizes (DC2PC 0x2B20; DC4/PH 0x56FE4), original-script
checkpoint offsets, local variables, resource names, sprite transforms and
timestamps. A checksum-bound `.dat.arisa` companion preserves emulator audio
positions and VM details. PC edits invalidate the companion automatically.
Native imports are verified independently of companions in scene tests.
Animated resource slot 33 and other unsupported renderer state are not fully
restored. A real DC2PC import/export sample was loaded in the original PC
executable, displayed Maya's dialogue/scene and advanced to the next line.
Plus Harmony loaded the corrected export with centred artwork and advanced
to the next dialogue. DC4 and Plus Harmony custom launchers were subsequently
verified without a DVD request; both loaded native-only samples and advanced.
The current DC4 export also restored its background and character placement.
These samples do not establish compatibility for every renderer/history state.
Unsupported export layers fail before replacing a slot.
Choice lists resume at their wait command and retain working branch targets.
Validation uses an independent scene before committing to live state.
Unknown binary layouts/checkpoints remain unsupported instead of guessing
offsets from a title string. Prefix reconstruction currently covers sprite
transforms; native stepped motion is still settled immediately.

Portable checks cover corruption, hidden stale bodies, local-variable
preservation, different speakers and choice branches. `DC2PC_PC_SAVES`
enables installed-data checks for copied PC files; those saves and proprietary
assets are not checked in. Real tests run against both language resource sets
and launch the unlocked bonus stories.

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

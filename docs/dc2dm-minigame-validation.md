# DC2DM minigame implementation and validation

Scope: Da Capo II Dearest Marriage MES profile `0x9D72`. Other profiles must not dispatch the new minigame services or substitute score markers.

## Native behavior

The recording and native executable supply the visual and behavioral reference. Original CRX artwork supplies the board, stones, portraits, difficulty/order/replay buttons, thinking indicator, result panels, and quit confirmation. Layout remains on the original 1024 × 576 coordinate system, scaled to Arisa32's 1280 × 720 canvas.

The shared opening screen lasts approximately 4.5 seconds. Native flag `100[15]` permits skipping it for both games. BGM 214 starts after that screen; the original return script starts BGM 14. Suit text appears at approximately 30 rows per second, calibrated against result frames 650–658; choices wait until their rows have appeared. Reversi keeps the native one-second CPU delay and two-second result hold. Both games accept paired touches and keyboard selection; canceled or mismatched gestures do not choose an item.

Reversi places the new disk immediately, then recolors captured disks in distance waves after strictly more than 100 ms per wave. The CPU panel and one-second delay start at human placement and overlap these waves. Input is blocked during flips and CPU thinking; quit confirmation freezes both timers. Placement uses SE 398, or 399 at a corner; each capture wave uses SE 398. The original square selection remains in place underneath quit confirmation.

Reversi buttons retain the selected image while pressed. The native renderer at `0x43A79E` uses `184 * state`, while the input path writes only states 0 and 1. Activation at `0x43B3CD` records the choice without changing to the atlas's third column. The Android probe checks that selected and pressed images match, then verifies that canceled and mismatched touches do not activate the button.

Boot text, suit hand descriptions/history/statistic labels, and Reversi turn/pass/result messages use verified literals from the installed Indonesian executable. Original Japanese prompts and bitmap controls remain. The translated suit statistic formats contain ASCII markers while the original formatter searches fullwidth markers; this port renders the actual counters at those positions instead of displaying the broken markers. See `build/dc2dm-minigame-20261107/suit-analysis/contract.md`.

| State | Suit | Reversi |
| --- | --- | --- |
| Launch service | `200,0` | `200,1` |
| First-play flag | `100[14]` | `100[15]` |
| Cumulative player wins | `100[16]` | `100[18]` |
| Cumulative player losses | `100[17]` | `100[19]` |
| Result comparison | `201,0` | `201,1` |

Decisive completed games increment the session total once, capped at 255. Draws add neither a win nor a loss. Reversi counts results from the player's selected color. Suit ties append another hand prompt, while a decisive outcome offers replay/exit.

The user's requested interruption rule differs from immediate native persistence: session results are committed only when leaving through the minigame's own exit flow. Forced Title through Arisa32 or a shortcut, and application termination while the minigame remains active, discard that session's results. Earlier completed-session totals are retained. Config and pause saves cannot commit an active session. Confirmed Title tears down the controller and script wait so Load is available again.

## Dynamic Otome dialogue

Original `MGJK.MES`, `REVERSI.MES`, and `MINIGAME.MES` retain control of dialogue, speaker, voice clips, and return destinations. The first visit uses its introductory return branch. Later visits compare cumulative totals, writing `3[0] = 1` for more wins, `2` for more losses, and `0` for equal totals.

When `3[98]` selects suit or Reversi score text, fullwidth `＊` and `＠` are replaced with the corresponding fullwidth decimal totals before dialogue layout and history capture. ASCII `*` and `@` are preserved. Voice playback uses the script's existing recordings; no generated voice or invented dialogue is introduced.

| Return branch | Original Otome voice suffixes |
| --- | --- |
| First suit | 008, 009, 010, 011, 012 |
| Repeat suit, wins greater | 022, 025, 026, 029 |
| Repeat suit, losses greater | 023, 027, 028, 029 |
| Repeat suit, equal | 024, 029 |
| First Reversi | 014, 015, 016, 017, 018 |
| Repeat Reversi, wins greater | 031, 032, 035, 036, 039 |
| Repeat Reversi, losses greater | 031, 033, 037, 038, 039 |
| Repeat Reversi, equal | 031, 034, 039 |

The common resource prefix is `VOICE/MINIGAME/MINIGAME_OTM`.

Loading an older native save first seeds the new scene with current completed-session progress, so score text is reconstructed from the same totals retained by the profile.

## Fidelity limits

The CPU uses recovered Easy, Normal, and Hard decision paths, including native move-order and recursive-search fixtures. It does not reproduce the complete executable's global random-number stream; rare x87 versus portable floating-point ties can also choose a different equal-ranked move. The same move sequence as the recording is therefore not guaranteed. Font rasterization is portable rather than GDI pixel matching. The supplied recording aborts Reversi after 32 moves; natural wins/losses/draws and automatic passes are established from executable analysis and tests, not observed in that recording.

## Evidence and checks

- [Recording analysis](dc2dm-minigame-frame-analysis.md): extraction and change metrics for all 6,357 frames, contact-sheet review, enlarged state frames, and precise limits of recording coverage.
- `dc2dm_suit_test`: hand outcomes, ties, replay, counts, console/history limits, and row batches.
- `dc2dm_reversi_test`: legal moves, directional flips, pass/end conditions, player perspective, difficulty selection, and recovered CPU fixtures.
- `dc2dm_minigame_test`: controller input, boot/text/result timing, pause through quit confirmation, complete matches across difficulty/order combinations, one-time result accounting, and original artwork captures.
- `dc2dm_minigame_script_test`: all eight native return branches, score substitutions, voice order, return destinations, other-game counters, and non-DC2DM profile isolation.
- Android packaged-library probe: native Extras/menu entry, both game choices, three suit hands, three Reversi levels and both orders, touch cancellation, pause/resume, Config/Back, quit/continue, and confirmed Title.

The Android probe runs each phase under a device-side watchdog: termination at 110 seconds, forced termination by 115 seconds, and a 120-second local ADB limit. A timeout retains captured output and retrieves available screenshots with a separate 30-second limit. This prevents a failed phase from losing its evidence or leaving its remote process running after the local command stops.

## Final validation, 8 November 2026

| Check | Result | Evidence |
| --- | --- | --- |
| Desktop minigame tests | PASS, 4/4, 65.37 seconds | `build/dc2dm-minigame-20261107/desktop-final.log` |
| Remaining desktop regression tests | PASS, 27/27, 115.03 seconds | Same log; 31/31 tests in total |
| Java profile checks | PASS, 328 checks | `build/dc2dm-minigame-20261107/java-profile-checks.txt` |
| Android native build and packaging | PASS, ARM64 and ARMv7; Gradle completed in 4m 7s | `build/dc2dm-minigame-20261107/android-final.log` |
| Packaged ARM64 JNI on I2219 | PASS, all six phases: suit, Reversi Easy/Normal/Hard, menus, choices | `build/dc2dm-save-validation/3c878a79d8cd/result.log` |
| APK metadata, signature, alignment | PASS, package/version/ABIs checked, v1/v2 signatures verified, `zipalign -c 4` passed | `build/dc2dm-minigame-20261107/apk-validation.json` |
| Save integrity after probe | PASS, 8 PC files and 15 phone files match their original hashes | `build/dc2dm-minigame-20261107/save-integrity.json` |

The reproducible build and desktop test commands are in `build/dc2dm-minigame-20261107/final-android.ps1` and `final-desktop.ps1`. The desktop run sets `DC2DM_DATA` to the installed game and uses the copied PC saves. The Android build uses the existing `build/release-0.6.9/baseline-agp.init.gradle` to select cached AGP 8.5.2 in offline mode. The repository's AGP 8.13.2 setting remains intact.

The device runner is `tests/android/run_dc2dm_save_probe.py`, with cases `minigames-suit minigames-reversi-0 minigames-reversi-1 minigames-reversi-2 menus choices`. It uses the final APK below, copied PC saves, the generated native choice fixture, the bundled MSGOTHIC font, and serial `10DF1N05QN000H8`. The first probe failed because its assertion expected an empty response from successful MINI GAME navigation; JNI correctly returned `start`. Only that harness assertion changed before the successful complete rerun. Production code and the APK stayed unchanged. The failed evidence remains at `build/dc2dm-save-validation/393ae46903e3/`.

The suit device phase first commits a result through normal exit, then completes a decisive round in a fresh session and forces Title. It verifies that Load is enabled and that all four cumulative counters still match the earlier committed baseline. It loads a native save and checks those counters again. Reversi teardown and restored Load also have desktop coverage; the device probe does not complete a Reversi match and then force Title after that result.

The isolation test checks non-DC2DM versions `0xAAB6`, `0xABB6`, and `0x9969`: services must not start these controllers, change score banks, or substitute dialogue markers. This is regression evidence for those fixtures, not a claim that every game has been played through.

The original phone hash baseline accidentally included the directory `SaveData/dc2dm` with an empty-content hash. `adb ls -ld` confirms it is a directory, so the file comparison excludes that row. All 15 regular top-level files match. Nested contents of that directory were not part of the original baseline. The probes use isolated save fixtures.

Root visual review covered all five fresh device minigame contact sheets, containing 49 minigame captures, and full-size `suit-scored-before-forced-title.png`, `minigames-returned-title.png`, `minigames-title-loaded-choice.png`, `reversi-2-1-quit.png`, and `reversi-cpu-after-pause.png`. The board, original prompts, selected state, score result, restored Title, and loaded choice render correctly. Captures are under `build/dc2dm-save-validation/3c878a79d8cd/`. The desktop controller sheet and full-size quit/result captures were also reviewed.

The recording analysis extracted and hashed all 6,357 frames and inspected their changes, chronological contact sheets, and selected enlarged frames. It does not claim that every frame was individually viewed at full resolution.

## APK

- File: `build/apk/Arisa32-0.6.9-debug-DC2DM-minigames-20261108.apk`
- Package: `org.dc4engine`, version `0.6.9`, version code `16`, debuggable.
- Size: 16,328,412 bytes; ABIs: `arm64-v8a`, `armeabi-v7a`.
- SHA256: `d0e1829fb88190ff5679fd7cf879ac87bb5a9fc82a0020f05f7aff01eb271f20`.
- Signing certificate SHA256: `6cda1050c64fb9433f07095ab88027c2a521452ec219e9849279e26704acf686`.
- ARM64 `libarisa32.so` SHA256: `60d5e1a90d28ffbb20200144d7b5434a8f8543777e0751488ca7e915095c813a`.
- ARMv7 `libarisa32.so` SHA256: `63ba6ffbc38a186d78f1be905b2b135be7e50d67c9f9b94458ff122f0de91f48`.

The device probe records the same APK and ARM64 library hashes. It exercises the packaged JNI, not a manual traversal of the full Activity or listening to every audio clip. ARMv7 builds successfully but has not run on a 32-bit device. Voice-resource selection is tested; complete story playback and acoustic fidelity remain outside this validation.

## Device installation

PASS: `adb -s 10DF1N05QN000H8 install -r build/apk/Arisa32-0.6.9-debug-DC2DM-minigames-20261108.apk` returned `Success` on the connected I2219. The package manager reports version `0.6.9` / code `16`, ARM64, and update time `2026-11-08 00:48:46`. Installation updated the existing app without uninstalling or clearing its data.

The installed `base.apk` SHA256 equals both the final local APK and the APK used by the device probe: `d0e1829fb88190ff5679fd7cf879ac87bb5a9fc82a0020f05f7aff01eb271f20`. All 15 compared phone save files still match their pre-probe hashes after installation. Evidence: `build/dc2dm-minigame-20261107/installation.log`, `installation-verification.json`, `phone-saves-after-install.txt`, and `save-integrity.json`.

## Scoped Delivery Gate

Root performed this gate for the DC2DM minigame port and forced-exit correction. The user's direction is to preserve the PC game's UI and behavior. Original art, bitmap typography, console clipping, and menu styling therefore remain the reference; this gate does not redesign or certify the entire legacy game's accessibility. The descriptive dials are ENERGY 2 (Otome portraits and original controls), RHYTHM 1 (fixed board/console layout), MOTION 1 (state-driven text, flips, and CPU timing).

### Hard Gate

| Item | Status and evidence |
| --- | --- |
| R-02, prose | PASS: newly authored report and labels use concrete descriptions; original script text remains sourced game content. |
| R-03, layout | PASS within the requested port: reviewed 1280 × 720 device captures retain the board, choices, and modal inside the canvas. Native scrolling-console clipping is intentional. |
| R-17, statistics | PASS: displayed results come from the suit/Reversi state and tested score banks; reported test counts come from logs. |
| R-18, testimonials | PASS: none introduced. Otome is original game artwork. |
| R-23, assets | PASS: user requested the original PC presentation; the port loads existing CRX assets and creates no replacement artwork. |
| R-24, navigation | PASS: six device phases cover minigame entry, exit, Config, Title, Load, and restored choice. |
| R-25, contrast | PASS for new console text: white `0xffffff` on black, 21:1. Original bitmap controls/menu colors are preserved by user direction, not claimed as a new WCAG-compliant design. |
| R-26, controls | PASS: tests exercise hand selection, difficulty/order, board moves, replay, continue, exit, and canceled/mismatched gestures. |
| R-27, states | PASS: controller tests cover boot, missing assets, choice, thinking, result, pause, and teardown. |
| R-28, FAQ | PASS: no FAQ introduced. |
| R-32, keyboard/focus | PASS: controller tests use Next/Previous/Accept/Cancel; the original selected image and board cursor remain visible. |
| R-33, implementation | PASS: behavior lives in C++ sources, SceneRunner, JNI, and SDL integration; no runtime source-patching workaround. |
| R-34, themes | PASS: no theme switch introduced; original game art supplies the fixed appearance. |
| R-35, execution | PASS: both Android ABIs built, 31 desktop tests and six device phases passed; click/touch captures are retained. Full Activity coverage is explicitly excluded. |
| R-36, claims | PASS: build, hash, save, and test claims link to recorded evidence; limitations are stated above. |
| R-37, direction | PASS: the user supplied the PC recording and explicitly required matching game UI before implementation; no new visual concept was invented. |
| R-38, sourced content | PASS: game prompts, dialogue, voice paths, assets, and counters derive from the installed game and executable analysis. |

### Purpose-Gate

| Item | Status and evidence |
| --- | --- |
| R-01, gradients/glows | PASS: inherited title/choice art preserves the PC reference; no decorative gradients added. |
| R-04, icons | PASS: original controls and stones represent game actions and board state. |
| R-06, typography | PASS: console text preserves the native console presentation; bitmap button text comes from CRX assets. |
| R-07, grid | PASS: the eight-by-eight Reversi grid defines legal board coordinates. |
| R-08, arrows | PASS: suit's selection marker communicates the active choice. |
| R-09, badges | PASS: no badges introduced. |
| R-10, glass | PASS: no new glass effect; the inherited dialogue/choice art is the requested reference. |
| R-12, shadows | PASS: no new component shadows. |
| R-13, glow | PASS: selection uses native states/cursor, without adding glow effects. |
| R-14, cards | PASS: no feature-card layout; equal board cells represent equal game coordinates. |
| R-19, motion | PASS: row timing, disk flips, thinking delay, and result hold communicate actual game state; pause timing is tested. |
| R-22, illustration | PASS: original Otome portraits reflect native result/reaction states. |

### Liveliness

| Item | Status and evidence |
| --- | --- |
| Explicit dials | PASS: ENERGY 2 / RHYTHM 1 / MOTION 1 describe the preserved PC presentation. |
| Dial consistency | PASS: reviewed captures show a stable layout and limited game-state motion. |
| Focal point | PASS: board or current suit choice is primary; quit confirmation overlays that action. |
| Structural space | PASS: original board/console/portrait regions keep their distinct roles. |
| Accent | PASS: the active board cell or selected choice identifies the next action. |
| Identity | PASS: native Otome portraits, board art, prompts, and DC2DM menus recur across states. |
| Design reference | PASS: the supplied PC recording and executable established the reference before the port; frame analysis records it. |

### Craftsmanship and Quality Locks

| Item | Status and evidence |
| --- | --- |
| C-1, intention | PASS: layout/art preserve the supplied reference; timing and controller behavior follow recovered native contracts. |
| C-2, completeness | PASS: controller and device tests exercise the introduced interactive controls. |
| C-3, composition | PASS: each region serves the game board, console, portrait, or action prompt. |
| C-4, resilience | PASS within tested scope: touch cancellation, keyboard selection, pause, missing assets, normal exit, and forced Title are covered. |
| C-5, evidence | PASS: score and test claims are backed by state assertions and retained logs. |
| R-05, structure | PASS: native game layouts, without a new template shell. |
| R-11, shapes | PASS: board cells and original rectangular minigame controls retain their native shapes. |
| R-15, action labels | PASS: existing hand, difficulty, order, replay, continue, and exit labels describe their actions. |
| R-16, language | PASS: native game text and technical validation prose; no promotional copy added. |
| R-20, identity | PASS: the original Otome/CRX presentation remains recognizably DC2DM. |
| R-21, dark background | PASS: black console/board surround matches the PC minigames. |
| R-29, palette | PASS: original black/white/green board and native selection accents; existing character artwork preserved. |
| R-30, reference | PASS: deliberate port of the requested game's presentation, not an unrelated product template. |
| R-31, rationale | PASS: geometry, typography, art, timing, and selection reasons are recorded in this report and the frame analysis. |

The UI supplement passes within that scope: original art defines hierarchy and color, controls have tested behavior, and state-driven motion has a recorded purpose. The copy supplement passes for newly authored text: factual claims have sources, labels describe actions, and the report states validation limits without invented results.

# DC2DM native minigame recording

Source: `C:/Users/adamb/AppData/Local/Packages/Microsoft.ScreenSketch_8wekyb3d8bbwe/TempState/Recordings/20261107-1531-53.3385381.mp4`.

The recording is H.264, 1022 × 596, constant 30 fps, 211.900 seconds, 6,357 video frames. Frame numbers in this report are one-based: time is `(frame - 1) / 30`. Original media and game saves were not changed. The source is 72,980,905 bytes with SHA-256 `ebe033944079bd886222f62784547a94eed707bcd67380d596519e32f78dc401`.

## Evidence and coverage

All 6,357 video frames were extracted at source dimensions into `build/dc2dm-minigame-20261107/frames/00001.jpg` through `06357.jpg` using ffmpeg `-fps_mode passthrough -q:v 3`. JPEGs are lossy evidence copies; the original MP4 remains the primary source. The completed `analyze_frames.py` pass calculated successive-frame RGB changes from every JPEG after quarter decoding to 256 × 149. `frame-metrics.json` contains exactly 6,357 entries, continuous frame numbers 1–6,357, and one extracted-JPEG SHA-256 per frame. `source-manifest.json` records the source hash and continuity checks.

Visual inspection uses chronological contact sheets and enlarged key frames. It does not claim that every full-resolution frame was individually viewed. All six `overview-*.jpg` sheets were inspected: 106 thumbnails at two-second intervals from 0–210 s. The first review sheets are `early.jpg` (0–30 s, 16 frames), `middle.jpg` (32–104 s, 13 frames), `reversi-early.jpg` (110–140 s, 16 frames), and `reversi-late.jpg` (140–211.867 s, 16 frames). Key screens and transitions are enlarged separately.

The all-frame scan found 75 frames with a mean RGB change above 9/255 and produced four adaptive `changes-*.jpg` sheets. The 198 entries in `board-state-changes.json` are image-classifier changes, not a count of moves. Difficulty/CPU/quit graphics can cover cells and be misclassified as stones; those occluded states must not be used as gameplay counts.

The source itself is the design reference. This report describes the authored native UI, including its mixed Indonesian/Japanese text and debug-like output. It does not propose a replacement visual style.

## Coordinate system

The Windows title bar occupies approximately y=0–22. Game content begins at y=23. The PC game uses authored 1024 × 576 coordinates; the recording clips about two pixels on the right and three at the bottom. The Android engine renders at 1280 × 720, scaling these coordinates by 1.25. For coordinates below, capture y includes the title bar. Engine coordinates remove that title bar and apply the scale; they are not the original bitmap coordinates.

Neither minigame opens a separate Windows dialog. They use the existing game canvas. Suit is console-like text over a black background; Reversi combines that background with a board, status text, character portrait, and authored modal graphics.

## Suit / rock-paper-scissors

At 0 seconds the extras menu has MINI GAME selected. By 2 seconds Otome is in the bedroom with the normal message panel. Around 6 seconds the two normal visual-novel choices are visible: `Pilih suit` and `Pilih reversi`. Suit is chosen. At 12–16 seconds there is a black loading screen with a small white line beginning `Pelatihan Program Laboratorium Amakase, Tugas ...` and loading text beneath it.

The portrait and first active suit text appear at frame 00501 (16.667 s). The complete initial hand list is visible at frame 00504 (16.767 s):

- The full game canvas is black. White monospaced text begins at the upper left, with essentially no horizontal inset. Ordinary glyphs are approximately 16–18 capture pixels high and line spacing about 22 pixels during suit play.
- The hand prompt is `《じゃーん、けーん》`. Choices are separate text rows `【グー】`, `【チョキ】`, `【パー】`. The selected row has a `»` pointer at x=0. These are text choices, not three colored cards or standard operating-system buttons.
- Frame 00601 (20 s) has scissors selected. The hand rows are around capture y=140, 163, 185 in this first round. Later rounds start lower because the history heading and record line are present.
- The portrait frame is at capture x=769, y=312, extending to the recording's right/bottom edges. The engine rectangle is approximately x=960, y=360, width=320, height=360. The portrait sits inside a thin white/dark frame and remains fixed while text changes.
- The first observed decisive round is player scissors versus CPU rock and the player loses. Frame 00661 (22 s) already shows the choices, both hand lines, `《結果》`, `あなたの負けです`, and the retry prompt.
- The retry prompt is `《もう一度対戦しますか？》` with `【はい】` and `【いいえ】` on separate rows. The same `»` marks the current choice. In frame 01801 (60 s) it is on yes at capture y≈405.
- The heading `◆ Hasil Pertandingan` and a row of result symbols accumulate between rounds. Subsequent result screens contain mixed localized hand text such as `<<Anda: Kertas>>` and `<<CPU: Gunting>>`, while win/loss and retry instructions remain Japanese.
- Ties append another `《あーいこーで》` prompt and hand list to the current text flow. In the 68–74 s samples, multiple tied throws remain visible before a decisive result. Text scrolls when it extends below the canvas; it is not replaced by a centered tie modal.
- Portraits visibly change with state: neutral open eyes during choice, surprised/open-mouth during the CPU's win, and smiling closed eyes during the CPU's loss in observed result frames. Exact face-index mapping is an asset/code concern.

The first result is appended over several recorded frames. The 24 cropped views in `suit-reveal-boundaries.jpg` were inspected, with per-frame pixel measurements for 50 adjacent frames in `suit-reveal-metrics.json`:

| Frame | Time | Newly visible text |
| --- | --- | --- |
| 00501 | 16.667 s | Initial headings and `《じゃーん、けーん》` |
| 00502 | 16.700 s | Rock and scissors rows |
| 00504 | 16.767 s | Paper row completes the initial hand list |
| 00650 | 21.633 s | Player hand: scissors |
| 00652 | 21.700 s | CPU hand: rock, and `《結果》` |
| 00654 | 21.767 s | Player-loss text |
| 00655 | 21.800 s | Retry prompt |
| 00658 | 21.900 s | Yes/no retry rows |

The nine appended result rows, including blank rows, span about 267 ms from first to final visible text. This is consistent with roughly 30 console rows per second in this capture. The recording cannot establish the program's internal refresh rate, and some consecutive video frames duplicate a displayed state.

Suit has returned to the bedroom by 86 seconds. The surrounding story reports an overall tied score and continues its ordinary dialogue. The first minigame visit finishes around 100 seconds, then the extras/minigame flow is entered again.

## Reversi

The second visit selects Reversi. The black loading screen is visible at 114–118 seconds with `Pelatihan Program Laboratorium Amakase, Tugas 9: Reversi` and loading text.

### Board and persistent UI

At frame 03601 (120 s), the board and portrait are already visible behind difficulty selection. The board capture bounds are approximately x=192–768, y=23–599, clipped at the bottom. This maps to an engine rectangle x=240, y=0, width=720, height=720. There are eight cells per side, approximately 72 capture pixels / 90 engine pixels per cell. The white grid is about four capture pixels thick. The cells are solid green.

Stones are plain filled black or white circles, about 64 capture pixels / 80 engine pixels in diameter. Legal destinations are black squares, about 32 capture pixels / 40 engine pixels on a side. The current selection has a yellow outline about four capture pixels thick. Selection and legal-move state are independent: frame 05401 (180 s) highlights an occupied white square at row 6, column 3. A yellow outline therefore must not be treated as a valid-move marker.

The mouse often stays near capture x=468, y=307 while the highlighted board cell moves far away. This is consistent with keyboard movement; the video alone does not prove each key or bind the highlight to hover.

The left strip is about 192 capture pixels / 240 engine pixels wide. It shows white, small monospaced text: turn announcements, `CPU Berpikir`, counts, and numeric board rows. The text is visibly clipped at the strip's right edge. It accumulates and scrolls during play.

The right status block starts at capture x=768, y≈39, corresponding to engine x=960, y≈20. Its five rows show player color, CPU color, black count, white count, and move number. In the localized recording the labels mix Indonesian and Japanese. The same bottom-right portrait rectangle used in suit remains at x=769, y=312.

### Authored dialogs

| Screen | Evidence | Capture geometry and appearance |
| --- | --- | --- |
| Difficulty | Frame 03601, 120 s | EASY x=164–347, NORMAL x=420–603, HARD x=676–859; all y=263–358. Black/gray rectangle, thin red border, large white serif uppercase text. Approximate engine rectangles: x=205/525/845, y=300, width=230, height=120. |
| First/second | Frame 03661, 122 s | `先攻` x=250–433 and `後攻` x=592–775, y=263–358. Same black/red treatment, large white Japanese lettering. |
| CPU thinking | Frame 03781, 126 s | Center rectangle x=420–603, y=263–358 with two text rows `CPU` and `思考中`. The board and right status remain visible. No full-screen dimming. The portrait is a closed-eye smile in this sample. |
| Quit confirmation | Frames 05881–05971, 196–199 s | Outer black/red panel x=236–787, y=175–367 reads `ゲームを終了しますか？`. `終わる` button x=289–473 and `続ける` x=552–736, y=263–359. The hovered end button is gray; continue is black. Board and portrait remain visible behind the panel. |

These are authored bitmap-like controls. They are not native operating-system dialogs or replacement visual-novel choice capsules.

The first CPU panel occupies frames 03760–03790 inclusive, from 125.300 s to the next frame at 126.333 s: 31 frames, or 1.033 s. The quit panel occupies frames 05860–05981, from 195.300 s to the next frame at 199.367 s: 122 frames, or 4.067 s. The latter is the recorded user's dwell time, not an automatic quit timeout. `key-timing-metrics.json` measures every frame around these boundaries; `dialog-boundaries.jpg` contains the adjacent before/after frames.

### Gameplay seen

Coordinates below are one-based row/column from the top left.

- Frame 03721 (124 s): starting board. White r4c4 and r5c5; black r4c5 and r5c4. Legal squares r3c4, r4c3, r5c6, r6c5. Black/white counts 2/2, move 00. The player is black and CPU is white.
- Frame 03781 (126 s): player plays r3c4, black/white 4/1, move 01. The last-move/current selection square remains yellow during the thinking overlay.
- By 128 s the CPU has replied, counts 3/3 and move 02.
- Frame 04201 (140 s): black/white 4/4, move 04, selected empty r3c7. Legal square markers remain visible elsewhere.
- Frame 05401 (180 s): black/white 10/16, move 22, selected occupied r6c3.
- Quit confirmation at 196–199 s preserves black/white 15/21, move 32, with yellow selection r1c6.

The recorded match is aborted after 32 moves. It does not show a natural Reversi win/loss/draw, pass prompt, or replay result. The user chooses to end the match, then a mosaic transition returns to the bedroom around 200 seconds. Dialogue continues, the screen is black at 210 seconds, and the extras menu is back by frame 06357 (211.867 s).

### Consecutive-frame disk changes

Every frame from 03759–03767 and 03791–03798 was inspected at source resolution. These samples show delayed replacement of full-size disks. They do not show a squashed or rotating intermediate disk shape.

| Event | Frame/time | Visible state |
| --- | --- | --- |
| Human placement | 03760 / 125.300 s | New black disk at r3c4; CPU panel already visible; captured r4c4 remains white; displayed count is 3 black / 2 white. |
| Human flip pending | 03761–03763 / 125.333–125.400 s | The visible upper arc of r4c4 stays full-size white. The CPU panel covers much of this disk. |
| Human flip | 03764 / 125.433 s | Visible arc becomes black; old legal markers disappear; counts become 4/1. Placement-to-flip interval is 133 ms. |
| Human-result face | 03765 / 125.467 s | Portrait changes from neutral to smiling. |
| CPU placement | 03791 / 126.333 s | CPU panel disappears; new white disk at r3c5; r4c5 remains a full-size black disk; counts show 4/2. |
| CPU flip pending | 03792–03795 / 126.367–126.467 s | The unobscured r4c5 disk stays full-size black. |
| CPU flip | 03796 / 126.500 s | r4c5 changes directly to full-size white; counts become 3/3. Placement-to-flip interval is 167 ms. |
| Player turn ready | 03798 / 126.567 s | Black legal-move markers appear and the portrait returns to neutral, 67 ms after the visible flip. |

The human CPU-thinking panel spans the human flip: it appears on the placement frame, before the captured disk changes color. The 30 fps recording bounds visible timing to about 33 ms and does not establish whether a shorter intermediate state existed between captured frames.

## Validation scope

Evidence extraction, source dimensions, frame numbering, and the observed native screens are documented. This analysis did not launch a new PC match, change production rendering, build an APK, or alter saved progress.

The separate Android JNI probe adds `--cases minigames`, expanded into four bounded processes: `minigames-suit` and `minigames-reversi-0/1/2`. Each enters through the existing Extras menu and native script choice, then creates a runtime save only in its isolated validation directory. It covers all suit hands, each Reversi difficulty and order, CPU response, canceled/mismatched/outside touches, occupied cells, pause stability, Back/Continue/Exit, Config/confirmation routing, and confirmed Title navigation. Later extensions check that forced Title discards a scored suit round while preserving previous saved totals, then allows loading the isolated choice save. Reversi waits require legal markers after CPU disk flips before checking player input.

The initial probe version compiled against Android 36 and passed Python syntax validation; `probe-compile.json` identifies those source hashes. The subsequent Java extensions were not built or run by this analysis agent. Compilation of the final probe, device execution, and APK validation are owned by the main implementation task; this report makes no device-pass claim.

Antislop evidence gate: PASS for sourced visual direction, sourced geometry/observations, original media preservation, and explicit limits on visual coverage. No new product UI was designed in this analysis; mobile, keyboard, theme, contrast, and click-through claims are not made here.

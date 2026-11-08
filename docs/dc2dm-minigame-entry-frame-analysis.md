# DC2DM minigame entry recording

Reviewed on 2026-11-08. All 549 decoded frames of the supplied `dc2dmopen_minigame.mp4` recording were inspected in sequence. The recording lasts about 18.270 seconds and displays at 1072 × 480 after rotation. It uses variable frame timestamps, so a frame count divided by 30 does not measure the delay.

The recording shows the Extra menu labels disappearing while its background remains stationary for about 1.27 seconds before fading. Android's scene-entry code adds a 1,200 ms wait to this path. `MINIGAME.MES` already provides the outgoing and incoming effects, so the host wait and extra fade duplicate work outside the script.

## Evidence

The [evidence directory](../build/dc2dm-minigame-entry-20261108) contains `source.json` with the original file hash, `probe.json` with timestamps, all 549 extracted frames, 19 consecutive contact sheets, per-frame change metrics, and the extraction script. Frame numbers below are one-based.

| Frames | Original timestamps | Observation |
|---|---|---|
| 1 to 85 | 0.000 to 2.971 s | Launcher and launch selection. |
| 86 to 87 | 3.004 to 3.023 s | Startup overlay, followed by the game title. |
| 138 to 139 | 4.343 to 4.377 s | Title options change to Extra options without a captured black frame. |
| 198 to 199 | 5.808 to 7.380 s | Music Mode appears. The 1.572 s timestamp gap contains no decoded intermediate frames. |
| 210 to 211 | 8.291 to 8.789 s | Music Mode returns to Extra. |
| 245 to 246 | 9.576 to 9.610 s | Scenario Mode appears. |
| 261 to 262 | 10.360 to 10.377 s | Scenario Mode returns to Extra. |
| 317 to 318 | 11.726 to 11.744 s | CG Mode appears. |
| 338 to 339 | 12.427 to 12.462 s | CG Mode returns to Extra. |
| 389 to 390 | 13.671 to 13.722 s | Extra options disappear after the minigame selection; the title artwork remains. |
| 390 to 442 | 13.722 to 15.006 s | The title artwork remains bright with no menu labels. Last bright frame is 442. |
| 443 to 444 | 15.023 to 15.055 s | The title artwork darkens, then the game canvas is black. |
| 445 to 447 | 15.275 to 15.705 s | Otome and the room appear, brighten, and gain the dialogue box. |
| 448 to 549 | 15.738 to 18.239 s | The introductory dialogue continues. The recording ends before suit or Reversi selection. |

The waiting interval and fade are visible on [sheet 13](../build/dc2dm-minigame-entry-20261108/consecutive-13.jpg), [sheet 14](../build/dc2dm-minigame-entry-20261108/consecutive-14.jpg), and [sheet 15](../build/dc2dm-minigame-entry-20261108/consecutive-15.jpg). The complete sheets cover the launcher, all shown menu visits, and the resulting dialogue.

## Script and host boundary

`MAIN.MES` transfers to `MINIGAME.MES`. In its `rnew` path, raw `0D 2,0` at offset 197 handles the outgoing fade. Raw `0D 32` at 200 snapshots the next composition, followed by `BG/BLACK`. The `new` path stages `BG032_D_NO` and Otome, then raw `0D 1,0` at 321 reveals them. The following raw `52` at 324 starts BGM 14 in mode 0. Native variables can enter `new` directly, so every visit need not perform the `rnew` outgoing fade.

The fix passes the current Extra framebuffer into the DC2DM SceneRunner and starts this script immediately. Its authored commands control the effects. The Android host no longer adds the unrelated 1,200 ms Start wait. SDL receives the same source frame and keeps the script's first timed transition. Suit/Reversi boot screens, CPU thinking, dialogue, and result timing remain in their existing script/controller paths.

## Limits

The video has no input-event log and has long gaps between some captured frames. It establishes the visible stationary-background interval, not precise tap-to-response latency or a reliable speed comparison against every other menu. The code identifies the extra 1,200 ms wait independently. Runtime checks for the patched APK are recorded in the combined validation report.

R-02 PASS: report prose contains no em dash. R-17/R-36/C-5 PASS: timings and observations cite the supplied recording and distinguish missing frames from rendered waiting. R-37 PASS: the original game UI and the user's request define the direction; this change introduces no replacement artwork or menu layout.

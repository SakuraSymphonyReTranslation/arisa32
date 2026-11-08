# Da Capo II Dearest Marriage: Start confirmation recordings

The Android recording shows an extra **“Kamu yakin?” / Yes / No** confirmation before an already unlocked three-story selector. The PC recording returns to the title menu and opens that selector directly from Start. The Android popup is a confirmation drawn with the game's pink artwork; it is not an Android toast or notification.

This analysis applies only to the **Da Capo II Dearest Marriage** profile. It describes the supplied recordings and native script branches. It does not establish that a new APK or device test has passed.

| Source | Decoded video | Duration |
| --- | --- | --- |
| [Android: dc2dmstart.mp4](C:/Users/adamb/Downloads/dc2dmstart.mp4) | 435 frames; H.264; stored 480 × 1072 with −90° display rotation, decoded as 1072 × 480 | Video 14.447489 s; container 14.482563 s |
| [PC: dc2dmstartpc version.mp4](<C:/Users/adamb/Downloads/dc2dmstartpc version.mp4>) | 221 frames; H.264; 1020 × 572; 30 fps | Video 7.366633 s; container 7.935979 s |

The attached [20261108-0014-58.1686887.mp4](C:/Users/adamb/AppData/Local/Packages/Microsoft.ScreenSketch_8wekyb3d8bbwe/TempState/Recordings/20261108-0014-58.1686887.mp4) is byte-identical to the PC file: both are 2,533,752 bytes with SHA-256 `2c5a125e012da0ceb71988c8c423891f945248b189494a6138cf36cb94a6ca5c`. The Android file is 1,737,359 bytes with SHA-256 `1110dec3f36fb9fba75b020f93f9509e4e1450497d185e46592f91acca5dea4a`. The duplicate attachment is not a third independent observation.

All **656 frames** were visually inspected in consecutive contact sheets: Android frames 1–435 and PC frames 1–221, without interval sampling. Individual full-size frames and unscaled menu crops were also inspected at popup and menu boundaries. The JPEG evidence preserves displayed dimensions, but is not lossless. Frame numbers are one-based; Android timestamps below come from each frame's `best_effort_timestamp_time`, because its frame intervals vary. PC timestamps are `(frame − 1) / 30`.

| Android frames | Recorded time | Observed behavior |
| --- | --- | --- |
| 1–9 | 0.000–0.199 s | Title menu; Start is highlighted before the popup. |
| 10–38 | 0.243–1.248 s | First “Kamu yakin?” confirmation. No is highlighted at frame 38. |
| 39–89 | 1.264–2.675 s | Popup closed; title menu remains. |
| 90–129 | 2.726–3.862 s | Second confirmation. Yes becomes highlighted at frame 120, 3.610 s. |
| 130–177 | 4.007–5.273 s | Confirmation is gone; the title menu remains visible. |
| 178–179 | 5.307–5.324 s | Title darkens, then the game area becomes black at frame 179. |
| 180–233 | 6.024–7.447 s | Three story choices plus Back: Graduation days, Marrid life, and Dear to…… . Back is highlighted before leaving. |
| 234–241 | 7.461–8.024 s | Selector darkens; frames 235–239 are black in the game area; the title reappears dimly at frame 240. |
| 242–285 | 8.040–9.258 s | Title restored; Start is highlighted before the third popup. |
| 286–315 | 9.291–10.211 s | Third confirmation. Yes becomes highlighted at frame 306, 9.976 s. |
| 316–356 | 10.324–11.606 s | Confirmation is gone; title menu remains. |
| 357 | 11.640 s | Title is darkened. The next captured frame is about 0.702 s later; there is no separately captured black frame in this second entry sequence. |
| 358–435 | 12.342–14.412 s | Three-story selector remains until the recording ends. |

The first confirmation is cancelled; the next two are accepted and lead to the same unlocked selector. No story is completed during the clip. Consequently, these are repeated entries into one visible progress state, not demonstrations of three different completion stages.

| PC frames | Recorded time | Observed behavior |
| --- | --- | --- |
| 1–69 | 0.000–2.267 s | Recording begins inside the three-story selector. The pointer moves among its items. |
| 70–90 | 2.300–2.967 s | Return toward the title menu; outgoing and incoming menu text overlap. Blocky smearing obscures parts of this interval. |
| 91–144 | 3.000–4.767 s | Title menu is visible. The image is initially blocky, then clears; Start is highlighted. |
| 145–180 | 4.800–5.967 s | Start opens the three-story selector directly. Both menus' labels are visible while the title labels fade; the sakura background stays visible. There is no Yes/No confirmation or intervening black game screen. |
| 181–221 | 6.000–7.333 s | Three-story selector only. Frame 181 begins another burst of blocky image degradation that subsequently clears. |

Neither PC direction has a captured full-black intermediate frame. Android does show black when entering the selector the first time and when returning to the title. The PC capture also retains faint old labels while the pointer already highlights new choices, and has abrupt blocky degradation across the logo/background. Those remnants and compression artifacts must not be used as an exact engine-duration specification or recreated as an intended effect. The reliable visual requirement is a direct menu transition with the background visible, without the extra confirmation in the unlocked state.

The videos both show three story entries, plus the separate Back command. They do **not** show Start before Graduation Days is completed, or the intermediate two-story selector. Those stages are established by the user's requested rule and the native script audit:

| Progress state | Start behavior required for this profile | Evidence |
| --- | --- | --- |
| `100[1] == 0` | Keep the existing Start confirmation, then begin Graduation Days after Yes. | Keeping confirmation is the user's explicit rule. `sceselect.mes` at offset 1353 checks `100[1] == 0`, then jumps to `start_sce01` at 1368; that route transfers to `MAR_0101_F1` at 2278/2292. |
| `100[1] != 0`, `100[2] == 0` | Start directly opens Graduation Days and Married Life, plus Back. | Passing the initial gate reaches `sce01` at 1615/1624 and `sce02` at 1697/1706. The false branch of the `100[2]` check at 1779 places Back at 1956/1965 without adding a third story. |
| `100[1] != 0`, `100[2] != 0` | Start directly opens all three stories, plus Back. | The true branch at 1779 adds `sce03` at 1791/1800 and Back at 1873/1882. This matches the progress state visible in both recordings. |

The `100[1]` gate comes first, so `100[2]` alone must not bypass it. The native title path at `main.mes` offsets 2263–2277 reaches `SCESELECT` at 3772/3784; that inspected path has no explicit Yes/No branch. The native script evidence therefore establishes selector gating, while retaining the initial-stage confirmation follows the user's requested behavior. The displayed asset spells the second item “Marrid life”; “Married Life” above names that same story.

Native references: [sceselect disassembly, initial gate](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/native/mes/sceselect.txt:176), [selector entries](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/native/mes/sceselect.txt:207), [third-story gate](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/native/mes/sceselect.txt:227), and [title Start entry](E:/Games/Adv32roid/build/dc2dm-scene-transition-20261108/native/mes/main.txt:290). Numeric script offsets are decimal byte offsets, not text-file line numbers.

Evidence is saved under [dc2dm-start-20261108](E:/Games/Adv32roid/build/dc2dm-start-20261108):

| Evidence | Coverage |
| --- | --- |
| [Source manifest](E:/Games/Adv32roid/build/dc2dm-start-20261108/source-manifest.json) | Exact paths, sizes, and SHA-256 hashes for all three supplied paths |
| [Android probe](E:/Games/Adv32roid/build/dc2dm-start-20261108/android-probe.json) / [PC probe](E:/Games/Adv32roid/build/dc2dm-start-20261108/pc-probe.json) | Video metadata and timestamp for every decoded frame |
| [Android metrics](E:/Games/Adv32roid/build/dc2dm-start-20261108/android-frame-metrics.json) / [PC metrics](E:/Games/Adv32roid/build/dc2dm-start-20261108/pc-frame-metrics.json) | Frame timestamps, adjacent-frame RGB change, and JPEG hashes |
| `android-consecutive-1.jpg` through `android-consecutive-15.jpg` | Every Android frame, 30 frames per sheet except the final 15 |
| `pc-consecutive-1.jpg` through `pc-consecutive-8.jpg` | Every PC frame, 30 frames per sheet except the final 11 |
| [Android boundary crops](E:/Games/Adv32roid/build/dc2dm-start-20261108/android-menu-transition-boundaries.jpg) / [PC boundary crops](E:/Games/Adv32roid/build/dc2dm-start-20261108/pc-menu-transition-boundaries.jpg) | Unscaled menu crops around confirmations and changes |
| [Android overview](E:/Games/Adv32roid/build/dc2dm-start-20261108/android-overview-1.jpg) / [PC overview](E:/Games/Adv32roid/build/dc2dm-start-20261108/pc-overview-1.jpg) | Navigation aids; complete coverage is in the consecutive sheets |

Representative full-size frames: [Android confirmation, 10](E:/Games/Adv32roid/build/dc2dm-start-20261108/android-frames/00010.jpg), [Android black screen, 179](E:/Games/Adv32roid/build/dc2dm-start-20261108/android-frames/00179.jpg), [Android selector, 180](E:/Games/Adv32roid/build/dc2dm-start-20261108/android-frames/00180.jpg), [PC title, 144](E:/Games/Adv32roid/build/dc2dm-start-20261108/pc-frames/00144.jpg), [PC direct blend, 150](E:/Games/Adv32roid/build/dc2dm-start-20261108/pc-frames/00150.jpg), and [PC recording degradation, 181](E:/Games/Adv32roid/build/dc2dm-start-20261108/pc-frames/00181.jpg).

The evidence scripts are [analyze_start.py](E:/Games/Adv32roid/build/dc2dm-start-20261108/analyze_start.py), which computes metrics and consecutive sheets from the decoded JPEGs, and [inspect_start_boundaries.py](E:/Games/Adv32roid/build/dc2dm-start-20261108/inspect_start_boundaries.py), which creates the unscaled boundary crops. Audio assets, ending playback, story completion, and new APK behavior are outside this frame comparison.

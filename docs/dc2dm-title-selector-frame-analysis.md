# DC2DM title and story selector recording analysis

Reviewed 2026-11-08. The supplied PC recording is the visual reference. This report describes recorded frames, not a frame-perfect comparison of the two engines. The review covered all 1,556 decoded frames: 910 Android frames and 646 PC frames, with the transition boundaries identified below.

The PC recording keeps the title background and logo visible at normal brightness while the title options dissolve into the three-story selector. After a story selection, the background, logo, copyright, and story labels dim together. The Android recording instead inserts black during Title/selector navigation and leaves bright story labels above the story-entry fade. Those labels also reappear when its opening is interrupted.

## Sources and method

| Recording | Source | Decoded frames | Timing and size |
|---|---|---:|---|
| Android | `dc2dm_start_show.mp4` | 910 | About 30.496 s; variable frame timestamps; encoded 480 x 1072, displayed 1072 x 480 after rotation |
| PC | `20261108-0241-48.5093270.mp4` | 646 | About 21.533 s; 30 fps; 1020 x 596 |

Evidence is in [the recording directory](../build/dc2dm-title-selector-20261108/recordings). It contains the source hashes in [source-manifest.json](../build/dc2dm-title-selector-20261108/recordings/source-manifest.json), original frame timestamps in each `*-probe.json`, every decoded JPEG in `android-frames` and `pc-frames`, consecutive contact sheets, and [analyze.py](../build/dc2dm-title-selector-20261108/recordings/analyze.py). Frame numbers are one-based and match JPEG filenames. Times below use the original recording timestamps, not a synthetic 30 fps Android timeline. No PC game executable was run and no PC saves were changed for this review.

## Android observations

| Action | Recorded boundary | Visible result |
|---|---|---|
| Title to selector | Frame 120, 3.478 s, darkens; frame 121, 3.509 s, is black; frame 122, 4.095 s, restores the selector | The three story labels and BACK appear bright over a still-dark title background. The background then brightens. See [sheet 05](../build/dc2dm-title-selector-20261108/recordings/android-consecutive-05.jpg). |
| Selector BACK to Title | Frames 161 through 164, 5.446 to 5.928 s, are black; Title returns at frame 165, 5.948 s | A black interruption separates the menus. See [sheet 06](../build/dc2dm-title-selector-20261108/recordings/android-consecutive-06.jpg). |
| Select a story | Frames 455 through 463, 15.497 to 15.961 s | The background fades while all story labels remain bright. Frame 463 retains labels on black; frame 464, 16.260 s, switches to white opening content. See [sheet 16](../build/dc2dm-title-selector-20261108/recordings/android-consecutive-16.jpg). |
| Interrupt the opening | Frame 743, 24.595 s, onward | Stale selector labels reappear against dark/black content. From frame 751, 24.830 s, the return effect breaks up those labels. They are gone by frame 780, 25.646 s; Title is visible at frame 790, 26.461 s. See [sheet 25](../build/dc2dm-title-selector-20261108/recordings/android-consecutive-25.jpg), [sheet 26](../build/dc2dm-title-selector-20261108/recordings/android-consecutive-26.jpg), and [sheet 27](../build/dc2dm-title-selector-20261108/recordings/android-consecutive-27.jpg). |

## PC observations

| Action | Recorded boundary | Visible result |
|---|---|---|
| First Title to selector | Title at frame 46, 1.500 s; label dissolve at frames 47 through 58, 1.533 to 1.900 s | The title options blend into the three story options and BACK. The background, logo, and copyright stay visible without a black interruption. See [sheet 02](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-02.jpg). |
| First story selection | The full composition dims from frame 123, 4.067 s; frames 151 through 170, 5.000 to 5.633 s, are black | Story labels fade with the background. White story content starts at frame 171, 5.667 s. See [sheet 05](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-05.jpg) and [sheet 06](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-06.jpg). |
| First story continues | Frames 172 through 254, 5.700 to 8.433 s | White gives way to tree, school, path, and fountain imagery. A cafe appears at frame 230, 7.633 s, and brightens; dialogue appears at frame 255, 8.467 s. No selector labels remain. See sheets 06 through 09. |
| Gameplay return to Title | Side menu opens at frame 271, 9.000 s; confirmation appears at frame 301, 10.000 s, and closes at frame 321, 10.667 s | A circular breakup effect covers the cafe and its side menu from frame 327, 10.867 s. It diminishes through frame 350, 11.633 s; the scene is visually black by frame 351, 11.667 s. Black continues through frame 394, 13.100 s. Title fades in from frame 395, 13.133 s, and is bright by frame 406, 13.500 s. See [sheet 11](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-11.jpg) through [sheet 14](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-14.jpg). |
| Second Title to selector | Frames 473 through 480, 15.733 to 15.967 s; selector fully visible at frame 481, 16.000 s | Another label dissolve keeps the background and logo bright and continuous. See [sheet 16](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-16.jpg) and [sheet 17](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-17.jpg). |
| Second story selection | Frames 550 through 574, 18.300 to 19.100 s | All title/selector artwork dims together. Frames 575 through 594, 19.133 to 19.767 s, are black; white opening content begins at frame 595, 19.800 s. See [sheet 19](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-19.jpg) and [sheet 20](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-20.jpg). |
| Opening continues to recording end | Frames 595 through 646, 19.800 to 21.500 s | White gradually reveals sky and petals from about frame 608, 20.233 s. Opening text belongs to that sequence; no selector labels appear. See [sheet 21](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-21.jpg) and [sheet 22](../build/dc2dm-title-selector-20261108/recordings/pc-consecutive-22.jpg). |

## Fix criteria and limits

For DC2DM, preserve the shared title artwork during Title/selector navigation and dissolve only the menu options. Apply the story-entry fade after composing all visible title and selector elements. Opening interruption must force Title and clear selector state before any return transition can capture or display it.

The first two criteria follow the visible PC behavior. Applying the same menu behavior to both two-story and three-story layouts, and forcing Title after opening interruption, are user requirements. The supplied PC recording shows the three-story layout and a gameplay return to Title. It does not show selector BACK, the two-story layout, or an opening interruption, so it cannot establish the precise original animation for those cases. Its ordinary gameplay breakup effect should not be treated as evidence for how an opening abort should look.

These recordings differ in capture rate, scaling, and timing. Exact elapsed durations and pixel equality are not acceptance criteria from this evidence. The observable criteria are continuity of shared artwork, inclusion of labels in the fade, and absence of stale selector content on return.

Documentation checks: R-02 PASS, prose contains no em dash. R-17/R-36/C-5 PASS, frame counts, timestamps, and observations cite decoded source evidence; unobserved behavior is labeled. R-37 PASS, the supplied original PC UI defines the direction. R-35 PASS for this static report, recording frames and metadata were inspected; this report makes no runtime-validation claim about a patched build.

[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.7.0 - PC Saves, Minigames, and DC2DM Fixes

This November 8, 2026 release extends D.C. II Dearest Marriage support with PC saves, rock-paper-scissors and Reversi, and fixes for voices, story choices, the gallery, and transitions. These game-specific rules apply only to the DC2DM profile. DC2DM support remains beta; fixes from 0.6.9 remain included.

### PC saves and story progress

- DC2DM slots now read and write paired `.dat` and `.dat.arisa` files. The companion retains full history and additional Arisa32 state, including audio playback positions. PC saves without a companion and existing portable `.dc4sav` saves remain loadable.
- Arisa32 imports `system.dat` so title backgrounds, story segments, music, CG, and Scenario Mode follow game progress. Completed progress survives an app restart or loading an older slot.
- Slot numbering follows PC: `DC2DM01.dat` for quicksave, `DC2DM02.dat` for autosave, `DC2DM03.dat` as the PC latest-save copy, and `DC2DM04.dat` through `DC2DM103.dat` for 100 regular slots. Script checkpoints create autosaves; gallery replay does not overwrite them.
- Save/Load rows display dates from the save contents and scenario names, including 279 MAR/SIN/SUC titles. Titles from translated executables can override the bundled Japanese names.

Keep `.dat` and `.dat.arisa` together to retain the full Arisa32 state. Arisa32 writes global progress to `system-profile.dat`; exporting it back to `system.dat` is not supported yet. Saving a slot does not refresh the separate `DC2DM03.dat` copy. Loading exported slots directly in the PC executable has not been verified.

### Start and story order

| Progress | Start behavior |
| --- | --- |
| Graduation Days unfinished | Shows confirmation before starting Graduation Days |
| Graduation Days completed | Opens Graduation Days and Married Life directly, without another confirmation |
| Graduation Days and Married Life completed | Opens all three stories, including Dear to, without another confirmation |

Normal play continues from Graduation Days into Married Life, then Dear to and the credits. Reaching the credits does not require opening every CG thumbnail or playing every Scenario Mode branch. The original second-story button reads `Marrid life`. The initial confirmation is retained as requested; the original script establishes the two/three-story unlock conditions.

### Menu voices and dialogue replay

- Start, Load, Extra, Scenario Mode, CG Mode, Music Mode, Mini Game, CONFIG, and Quit announcements use the appropriate assets for Otome's A/B/C voice styles. The `Nothing` option turns announcements off.
- Changing Otome's style plays the correct sample. Redrawing CONFIG no longer plays every button's voice. Accepted Save and Load actions have their respective voices; opening Save remains silent, matching the reference recording.
- Replay belongs only to the voice actually played for the current line. Preloading the next voice does not associate it with that line. Replay is inactive for unvoiced narration and Yoshiyuki lines, including after loading a save.

### Story choices and the CG gallery

- Every DC2DM story choice uses the PC artwork for normal, hover, and pressed states. Group placement, caption size, hit areas, and background dimming follow the game reference. Canceling a touch does not select a row.
- Choices retain the targets in the original scripts. Target checks cover all discovered choice groups; direct branch tests cover both choices in the Anzu recording and their shared continuation.
- Full CG previews use the source image with interpolation at the requested display size. The original 1024 x 576 artwork remains the detail limit; this change does not generate new HD assets.
- Image changes follow the unlocked variants and order within the selected category, skip locked entries, and wrap to its beginning. Returning to thumbnails restores the last image's page.
- Fades follow the effect-speed and Effects Off settings. A tap during a fade finishes the transition first, so it does not skip another image.

### Rock-paper-scissors and Reversi

- Both minigames open through the original MINI GAME menu and use the game's controls, board, portraits, and prompts.
- Rock-paper-scissors handles all three hands, ties, replay, and exit. Reversi provides Easy/Normal/Hard, turn-order selection, legal moves, disk flips, automatic passes, and win/loss/draw results from the player's perspective.
- Otome's dialogue, voice recordings, and return destinations remain script-driven. Win/loss text uses the corresponding game's totals; draws add to neither total.
- Leaving through the minigame's own exit flow saves the session results. Forcing Title through Arisa32 or a shortcut discards the active session's results, retains earlier completed-session totals, and restores Load access. Pause and CONFIG do not commit an active session.

### Scene transitions

- Returning from a scene to Title uses the original circle atlas over the outgoing scene.
- Dissolves, fade-in/out, feather reveals, and scene masks use DC2DM behavior. The classroom transition covers the scene in white, then reveals the next classroom through the original mask.
- Pause stops effect time. Scripts wait for transitions to finish, and forcing Title clears masks and script waits so Load does not remain locked.

### Testing and limitations

Feature reports document recording analysis, original scripts and assets, desktop tests, and Android tests of the APK's JNI library with isolated save fixtures. See [saves and progress](../../dc2dm-save-progress-validation.md), [voices and CG](../../dc2dm-menu-audio-transition-validation.md), [choices and replay](../../dc2dm-choice-voice-validation.md), [minigames](../../dc2dm-minigame-validation.md), and [Start and transitions](../../dc2dm-start-scene-transition-validation.md). Release-package verification is recorded separately in the [0.7.0 report](../../release-qa-0.7.0.md).

- Testing does not cover a complete playthrough of every route, listening to every voice recording, or every device and patch. Font rasterization can still differ from PC.
- Title-to-story-selection still passes briefly through black; PC uses a direct crossfade.
- Some native effects, including tile particles, flashes/shakes, and specific sprite effects, are not fully reproduced. More complex scene sequences still need comparison.
- Reversi follows the analyzed decision paths, but does not reproduce the executable's complete random sequence. Equally ranked CPU choices can differ from the recording.
- Recorded device runtime checks use ARM64. ARMv7 is included in the build but has not been tested on a 32-bit device.

### Downloads and installation

[Open the v0.7.0 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.7.0)

- `Arisa32-v0.7.0.apk`: release package.
- `Arisa32-v0.7.0-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Install the update without uninstalling the app to retain its data. Android 5.0+; arm64-v8a and armeabi-v7a. Game files and translation patches are supplied separately.

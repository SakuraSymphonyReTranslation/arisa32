# Dal Segno continuation validation

Latest continuation: [21 November 2026 progress and remaining work](dalsegno-progress-2026-11-21.md).
The results below describe the earlier 19 November session.

Validation date: 2026-11-19. Active checkout: `E:\Games\Adv32roid`, branch
`main`, based on `c3eb9b625ced963c11d870173e6f7bb3d0b0b061`.
Dal Segno uses MES version `0x009F9A` and `MesProfileId::DalSegno`.

## Handoff review

The Antigravity handoff reported successful Android launch, CONFIG dragging,
END exit and fresh-profile D.S. locking on the iQOO Z9x. Those are prior-session
observations. This continuation checks the working source and desktop tests;
it does not repeat those device interactions or modify user saves.

The existing `scene_test --dalsegno` only loaded the title and printed its
controls. It did not exercise sliders and returned success even when a `CHECK`
failed. The opcode test also expected raw `0x43` to be JUMP and `0x4C` to be
LABEL, opposite to the implemented mapping and installed script evidence.

## Corrections in this continuation

- Corrected the Dal Segno LABEL/JUMP test expectations.
- Made `--dalsegno` return a failing exit code for failed assertions.
- Added fresh/unlocked title conditions, all 11 toolbar action hit targets,
  initial master/BGM volume checks and slider dragging through all five CONFIG
  tabs. Each of the 11 observed sliders is checked for minimum/maximum clamping,
  changed rendering, release action and capture release.
- Mapped Dal Segno raw `0x65` to PLAY_SE. The previous PLAY_BGM_ONCE mapping
  interpreted `65534/65535` as negative BGM indices and logged 12 errors during
  the title/CONFIG test. The SE dispatcher already supports these voice/SE stop
  sentinels. Added mixer regressions checking retained BGM and system effects.
- Removed the character-volume fallback that changed zero to 100. The existing
  DC2TY actor mute test exposed this regression in the handoff's shared audio
  changes; zero now reaches the mixer as mute.

## Installed script evidence

`main.mes` declares `start_main` with raw `0x43` at offset 3374 and assigns
`config` as a button action with raw `0x4C` at offset 4004. At offset 4078 it
compares bank 100, index 105 with zero before constructing the `ds` control.

The test observes exactly `sce_start`, `qload`, `load`, `extra`, `config`, and
`end` with flag 100[105] = 0. Setting that authored flag to one and reloading
the title adds the seventh action, `ds`.

`config.mes` selects its five tabs through 100[54] = 0..4. Its master slider
metadata at offset 6748 is raw `0x6A` with words `225,0,14,5`; the release action
is `seselplay`.

Raw `0x65` uses `65534,0,0,0` in `main.mes` at offset 3365 and in CONFIG at 15.
CONFIG uses `65535,1,0,0` at 42 and `250,0,0,0` at 19339. Its return sequence at
34804/34813 uses IDs 0 and 250. The installed NAMES.DAT contains 400 BGM and
500 SE records, including `se\250.pcm`. After correction, the same title/CONFIG
test emits no audio-index warnings.

## Verified results

| Check | Result and coverage |
| --- | --- |
| MSVC desktop build | PASS, seven test targets rebuilt in the existing build directory |
| `mesopcode_test` | PASS, existing decoder coverage and corrected Dal Segno operation checks |
| `mesvm_test` | PASS |
| `save_host_test` | PASS |
| `scene_test` | PASS, default fixture regressions |
| `scene_test --dalsegno <AdvData>` | PASS, six/seven title actions, 11 toolbar targets, 11 slider drags |
| `audio_test` | PASS, existing mixer/actor regressions and new Dal Segno stop sentinels |
| `dc4_route_test` | PASS with both installed DC4 and DC4PH data sets |
| `system_menu_test` | PASS, both games' progression/import/migration checks; full installed menu asset suite not enabled |
| Android `assembleDebug -ParisaAbi=arm64-v8a` | PASS, rebuilt APK includes the two audio corrections |
| Android host `ProfileChecks` | PASS, 493 assertions including Dal Segno MES detection with a large NAMES.DAT, neutral-directory library title, Da Capo/DSIF separation, English base content and phone scan path |
| `git diff --check` | PASS |

The Android build completed in 1m 8s with 39 tasks (5 executed). Output:
`android/app/build/outputs/apk/debug/app-debug.apk`. It was not installed by
this continuation. Gradle reported existing deprecation warnings.

Logs and script disassemblies are retained temporarily at
`C:\Users\adamb\AppData\Local\Temp\arisa32-codex-continuation-0f7ff86f30484d75976903eba15ca561`.
The default scene/audio fixture suites emit missing-UI notices for fixtures
that omit those assets; the Dal Segno suite emits no such errors.

## Scope of the results

The title tests prove that the menu follows the authored unlock flag. They do
not prove every heroine ending awards that flag correctly, automatically repair
imported Dal Segno progress, or validate a complete playthrough. The handoff's
claim about all heroine/Ame completion remains outside this test coverage.
DC4/DC4PH retain their separately implemented automatic profile validation.

Slider checks exercise scene variables, rendering and capture/release. They do
not verify Java/JNI touch dispatch or audible volume changes on a device.
Dal Segno native save round trips, prologue voice playback, textbox appearance
and END activity exit were not newly tested in this continuation.

Builds reuse `build/desktop` and standard Gradle output. Test fixtures, logs and
disassemblies are under the task's Windows Temp directory. Existing Antigravity
edits, local build configuration, cleanup work and user saves are preserved.

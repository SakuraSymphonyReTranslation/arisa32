# DC4 / DC4 Plus Harmony map routing, 2026-11-02

## Cause and correction

The four map selections award points in bank 5 indices 0..3. At the end of
`dc4_kyo20190214d.mes`, `0x1E(2,0)` selects the heroine group before the
script compares bank 3 index 0 and transfers into MIA, MIB, MIC or MID.
Arisa32 did not implement this service. The result therefore stayed at its
previous value, normally zero, and sent players into MIA despite map choices
that should select a different group.

The native executables use the same algorithm: read the four map scores,
keep the first highest score, and write its index to bank 3 index 0. Ties
favor the lowest index; the input scores remain unchanged.

| Executable | Service 2 handler | SHA-256 |
| --- | --- | --- |
| DC4.EXE | `0x409F2A` | `79dfeb2bd086d73197a9cfa94aad1a2d16f4200b3de06d7a8829833305d192e7` |
| DC4PHDL.exe | `0x40A37A` | `945255aa9bf301b2170c8489711fdbd201579019e435ff4541950d6632fd4a5d` |

The service dispatch tables at `0x40AAF8`/`0x40AAA8` and
`0x40AF48`/`0x40AEF8` respectively map subcommand 2 to these handlers.
Their `jle` instructions retain the earlier candidate on equal scores.
Disassembly and executable identities are retained locally under
`build/route-branching/`.

`MesVm::next()` now executes this service before the following comparisons.
It is limited to Modern MES versions `0xAAB6` and `0xABB6`. Other versions,
ClassicDM, and unrelated services such as credits continue to reach the host.

## Regression coverage

The new `dc4_route_test` has asset-independent cases for each winning group,
all pairwise ties, all-zero scores, high byte values, snapshot restoration,
unchanged input scores, and profile/service isolation.

With installed data, it follows every combination of the four maps through
the intervening chapters into the actual group-opening transfer:

- 81 sequences with three map options and 256 with four options.
- Each sequence runs uninterrupted and with VM snapshots restored at every
  map, for 674 paths per game and 1,348 paths across DC4 and DC4PH.
- The first map's labels are deliberately checked separately from display
  order: `label03`, `label01`, `label02`, `label04` award groups 0, 1, 2, 3.
- Twelve later heroine comparisons per game cover greater, smaller and tied
  scores in the four authored group splitters. These checks seed the scores
  at each splitter; they do not play every preceding heroine dialogue choice.

Before the correction, the same test reported both DC4 and DC4PH entering
`dc4_mia20190301a` instead of `dc4_mib20190301a`. After the correction, all
paths and comparisons pass. All 23 desktop CTest suites pass (84.08 seconds),
including the existing DC2DM and DC2PC synthetic coverage and installed DC2DM
scene/ending coverage.

To run the installed-data checks, set `DC4_DATA` and `DC4PH_DATA` to each
game's `AdvData` directory, build `dc4_route_test`, and run it or CTest.
The default CTest run uses only synthetic cases when those variables are unset.

The Android JNI probe in `tests/android/replay-probe/` additionally exercises
four successive choices through production touch handling and script
transfers. Its 24 cases include all four groups and two tied-score paths for
both versions, with and without saving and restarting the native host at
every map. `tests/android/run_replay_probe.py` runs the probe against libraries
extracted from the supplied APK in an isolated device directory.

On the connected arm64 phone, the published 0.6.3 debug library reproduced
the defect: `[1,1,1,1]` selected `ROUTE0` instead of `ROUTE1`. The rebuilt
debug APK passes all 24 route cases, plus the probe's nine existing replay
cases and its translation-space check. The probe uses `app_process`; it does
not install an APK, control the foreground Activity, or touch user saves.

| Android evidence | Value |
| --- | --- |
| Before log | `build/route-branching/android-before.log` |
| After log | `build/route-branching/android-after.log` |
| Before `libarisa32.so` SHA-256 | `98c9bd95bd936ec44fb19c0590a64946c0932d0922d642962bc2c3f4e90ca3a5` |
| Tested arm64 library SHA-256 | `d8216700ccd129b43a533eddee220473af837c3da66897c36cbfb400bb3d8833` |
| Debug APK SHA-256 | `6e5849c045aa880ed52c6dbdb3272fe609c27f8037e0729326d068198444c64f` |

The debug build contains arm64-v8a and armeabi-v7a libraries and retains
version 0.6.3/code 10. Gradle 9.2.0, the workspace's AGP 8.13.2 and Android
Studio's JDK 21 produced the APK; its signing certificate matches the existing
0.6.3 package. This is a local test build, not a new GitHub release. Runtime
testing of the armeabi-v7a library is outside this check.

## Existing saves

A save before the group-selection service can continue with its existing map
points and reach the corrected branch. A save already inside the wrong group
does not automatically move to another route; load a save from before the
common-route split. This fix does not reconstruct choices from a completed
or already misrouted playthrough.

The checks establish map selection, group routing, the four later comparison
sites, and the documented save/restore cases. They are not a full playthrough
of every heroine route or a comparison of all UI states with the PC game.

# DC4 / DC4 Plus Harmony progression

The emulator must start a new profile with no completed endings. The original
MES scripts award completion flags and decide which choices and title buttons
exist. A completed profile keeps its unlocks, including after loading an older
save slot or resetting configuration.

## Script evidence

Both installed game versions use these bank 100 flags:

| Flag | Authored use |
| --- | --- |
| 101-104 | Nino, Sorane, Hiyori and Shiina completion flags |
| 105-106 | Miu and Chiyoko completion flags |
| 107 | First Arisu completion; enables the additional Sorane and Arisu/Arisa choices |
| 108 | The other Arisu/Arisa completion |
| 112 | Map unlock, awarded when flags 101-104 are all set |
| 100 | D.C. title entry, awarded by `ed.mes` when flags 101-108 sum to at least eight |

`dc4_mid20190420e` and `dc4_sor20190501a` test flag 107 directly.
The first Arisu completion is written in `dc4_asa20200406a` in DC4 and
`dc4_asa20190513c` in DC4PH. Flag 108 is written in `dc4_asu20190506b`.
These checks use opcodes and script identifiers, independent of dialogue text.

At the Sorane choice in `dc4_sor20190501a`, flag 107 = 0 exposes only
`label01` (the installed Indonesian patch's "Aku tidak akan pernah menyerah.").
Once Arisu Ending 1 sets flag 107, `label02` ("Kali ini aku pasti akan
menyelamatkannya.") becomes available as the second option. The condition is
the global completion flag, not a playthrough counter. Starting New Game or
loading an older slot does not relock it on a completed profile. A PC screenshot
is reference material and does not establish the phone profile's completion.

The February 8, 9, 11 and 12 map scripts select the ordinary map artwork and
three choices while flag 112 is zero. Once it is set, they select the `_ASA`
artwork containing the extra character/location marker and offer four choices.
Rendering and input both use that same branch of the original script.

`main.mes` tests flag 100 before constructing the D.C. button. With that flag
zero, END occupies y=536. With it set, D.C. occupies that row and END moves to
y=592. The host now exposes flag 100 as zero if any ending flag 101-108 is
missing, so an inconsistent old profile cannot display or activate D.C.
Profile serialization and scene initialization use the same validated flag.
All eight endings alone still do not manufacture flag 100; `ed.mes` awards it.
No replacement artwork or custom title layout is needed.

The supplied Seiya-Saiga URLs returned empty bodies in this environment.
The conditions above were checked against both games' installed MES files;
they are not presented as independently verified quotations from those guides.

## Cause and fix

The VM treated missing flags 100-119 as one. Menu initialization and settings
reset also assigned ones, and profile loading replaced explicit zeros with
ones. Android additionally forced those flags on after loading a save.

DC4/DC4PH now retain zero for missing completion flags and preserve earned
flags across settings resets. Profile and PC imports merge completion flags
monotonically, so an older Android profile cannot overwrite newer PC progress.
After save loading or backlog rewind, hosts apply global completion flags to
the scene before it can copy stale slot flags back into the profile. Local map
scores and route choices are left in the restored scene.

The VM change applies to modern MES versions AAB6 and ABB6. Menu and restore
policy use the existing DC4 scenario-catalogue identification. Other menu
profiles keep their compatibility behavior.

## Existing profiles

The user's 2026-11-06 clarification establishes that only Nino and Hiyori are
complete on the phone, with Arisu in progress. The earlier claim in this document
that the phone profile was 100% complete was incorrect. Native PC `system.dat`
completion flags remain supported.

Profiles with all eight fabricated ending flags cannot be reliably distinguished
from genuinely completed profiles. The engine does not guess which endings to
erase. The phone's profile contained ones throughout 100-119, matching the old
default-unlock bug. Its targeted repair follows the user's stated progress:
preserve 101 (Nino), 103 (Hiyori) and 110 (ending credits/Extra); clear 100,
102, 104-109 and 112. Flag 109 is awarded at the end of the D.C. scenario, and
112 requires all four initial heroines. Unrelated flags, seen assets, read lines
and story slots are preserved. The full save directory is backed up first.

## Regression coverage

`dc4_route_test` checks fresh VM flags, each authored ending award, all 64
combinations of the four prerequisite flags and the last ending reached,
actual map choices, the flag-107 choices, and D.C. with every ending present
or one missing. All 1,348 map paths also check the selected map artwork and
reject nonexistent choice indexes.

`system_menu_test --progression` checks hidden and unlocked title controls,
native title rendering, fresh/partial/completed profiles, profile reload,
settings reset, PC import ordering, and restoring global progress over stale
slot state without changing map scores. The default test run includes both
games when `DC4_DATA` and `DC4PH_DATA` are set.
Regression cases also cover a stale D.C. flag with only Nino/Hiyori complete,
native imports missing each of the eight endings, persistence of the corrected
flag, a pending confirmation after prerequisites change, and restoring global
progress over a slot whose old 100-119 flags were all one.
`system_menu_test --map-progression` renders each map with the flag off and on
and checks that every touch target belongs to an available choice.

The Android production-JNI probe adds fresh and save/restart progression
checks for both MES versions. Test evidence and title captures are kept in
the ignored `build/dc4-progression/` directory.

## Validation (2026-11-02)

- All 24 desktop CTest suites passed, including DC2DM regression coverage.
- The final map-artwork assertions passed for all 1,348 paths across DC4 and
  DC4PH. Both games passed the ending and choice prerequisites above.
- Locked/unlocked title and all four map captures were visually inspected for
  both games. The gated marker is absent in the locked artwork; the fourth
  choice and its marker appear together when unlocked. Touch-target checks
  passed for all 16 map renders.
- The Android release build succeeded for arm64-v8a and armeabi-v7a. Its
  production arm64 JNI library passed all four fresh/save-restart progression
  checks, 24 map-routing cases, nine replay cases and the CP932 space check on
  the connected Android device. These used isolated fixtures, not user saves.
- The APK ZIP integrity and signing verification passed. The local test APK is
  `build/dc4-progression/Arisa32-dc4-progression-test.apk`, retaining version
  0.6.4/code 11. SHA-256:
  `05ccd7ec9dc7f120ef5533a806d0e76a11f55299de7b4dee351a84b6cc0ec7b6`.

## Sorane recheck (2026-11-04)

Rebuilt `dc4_route_test` and `system_menu_test` against the current core.
Both passed with `DC4_DATA` and `DC4PH_DATA` set to the installed game data.
This includes the one/two-option Sorane gate, 1,348 map paths, ending awards,
fresh/completed profiles, profile reload, settings reset and PC imports.
Logs: `build/comu-video/route-final.log` and `profile-final.log`. No route
implementation change was required. The guide URLs could not be fetched
on this recheck (HTTPS hostname mismatch; HTTP connection refused), so the
conclusion remains based on both installed MES scripts and executed tests.

The graphical captures use the desktop renderer with installed game assets;
the Android checks exercise production JNI with synthetic scripts. This does
not represent a complete heroine playthrough on the phone. Feature validation
did not change the installed app, user saves or published v0.6.4 release.
The subsequent v0.6.5 packages are covered by
[release verification](release-qa-0.6.5.md).

## Phone profile correction (2026-11-06)

The connected phone's Plus Harmony save directory was backed up to
`build/dc4-unlock-audit/phone-backup-20261106-170531/` (319 files, with a SHA-256
manifest). The targeted nine-flag repair was applied to `system-profile.dat`.
All 318 other files remained byte-identical. The original profile hash is
`f6e2a446eadab5b89c684920db32ab88b98523219c17c499970606c417f45a67`;
the repaired profile hash before app startup is
`88e03aef2667955ad45e48eb362bfe229c18c4c9bb8906215792434b4151bc08`.

- Both installed datasets passed `dc4_route_test` and
  `system_menu_test --progression`, including the new inconsistent-profile cases.
- All 63 native phone slots and their `.arisa` companions loaded through
  `SceneRunner::load_state` from the backup. Applying the repaired global profile
  preserved each script, instruction position, current line and local variables
  in banks 0-9; syncing each restored slot kept D.C. locked.
- Locked/completed title captures for both games were visually checked. The
  locked title ends at END; the completed title inserts D.C. above END.
- Android `assembleRelease` passed for arm64-v8a and armeabi-v7a. The resulting
  production arm64 JNI library passed nine replay cases, 24 map-routing cases,
  four fresh/save-restart progression cases and the CP932 translation check.
- The signed APK matched the installed app's signing certificate and was
  installed successfully with `adb install -r`, retaining version 0.6.8/code 15.
  Launching Plus Harmony on the phone showed CONFIG followed by END, with no
  D.C. entry. Evidence: `build/dc4-unlock-audit/phone-title.png`.

The APK also contains the sprite-motion changes documented in
[sprite-motion validation](dc4-sprite-motion-validation.md). These checks do not
claim a complete heroine playthrough or visual parity for every animation.

### Delivery gate for this change

- Hard gate PASS (R-17/R-18/R-23/R-36/R-38): captures come from installed game
  data and the connected phone; completion comes from the user's clarification.
- Interaction PASS (R-26/R-35/C-2/C-4): locked D.C. rejects activation, completed
  D.C. starts its authored scenario, and confirmation rechecks prerequisites.
  Builds, desktop regressions and production JNI checks passed.
- Purpose and direction PASS (R-31/R-37/C-1/C-3): the requested PC behavior uses
  the original title script and artwork; no replacement visual design was added.
- Evidence and source PASS (R-33/C-5): the guard is implemented in the menu
  source, tests cover old profile/slot imports, and save repair has a full backup
  and before/after hashes. The new comments explain the legacy flag constraint.

Logs, captures, repair manifests and the APK are under
`build/dc4-unlock-audit/`. The APK SHA-256 is
`5805b683669edcfa629769efa4dbd8017524f552116b29144c4f775cdf72bcdd`.

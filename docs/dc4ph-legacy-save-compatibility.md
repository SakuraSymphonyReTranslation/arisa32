# D.C.4 Plus Harmony legacy save compatibility

Verified on 2026-11-19 against copies of the player's phone files. The save
owner used Arisa32 throughout. The files are valid Arisa32 snapshots, not PC
saves or Kirikiri saves.

The manual profile repair below is historical. Subsequent automatic profile
validation, migration and phone verification are documented in
[dc4-progression.md](dc4-progression.md#automatic-profile-analysis-2026-11-19).

## Load failure

Both snapshots have outer format 4 and VM format 1, with valid payload
checksums. The phone's story MES files match the snapshots when identified
using the old engine's version parsing.

| Save | Script | Saved/legacy fingerprint | Current fingerprint |
| --- | --- | --- | --- |
| `slot1.dc4sav` | `DC4_SEN20181213C.mes` | `56904af9429a662e` | `f4787f136b62eb51` |
| `quicksave.dc4sav` | `DC4_SEN20181214B.mes` | `c21437224cb6d738` | `4976def4c3da67a5` |

The old MES reader included the first opcode in the four-byte version value.
These scripts begin with version `ABB6` and opcode `8B`, producing the old
value `8B00ABB6`. The current reader correctly separates the three-byte header
from instructions. Because the VM fingerprint includes the version value,
valid old saves were rejected as belonging to a modified script.

VM restoration now accepts either fingerprint calculation. Both calculations
still cover all script bytes and labels. New snapshots use the current
fingerprint. All other snapshot validation remains in place.

## D.C. unlocked at the prologue

In both supplied story slots, bank 100 flags 100-119 are zero. In the phone's
separate `system-profile.dat`, every flag in that range is one, including D.C.
flag 100 and all eight prerequisite ending flags 101-108.

This is consistent with the previously fixed default-unlock bug documented in
[dc4-progression.md](dc4-progression.md). Global completion is preserved when
loading an older story slot, so the prologue position does not relock D.C.
The flags do not prove that the save owner completed those routes. The initial
APK installation retained the profile; the user then requested that this unlock
problem also be repaired. The resulting profile repair is recorded below.

## Validation and phone installation

- `mesvm_test`, `scene_test`, and `save_host_test` passed. Regression coverage
  includes legacy restoration, normalization, corrupted legacy fingerprints,
  and rejection after bytecode, version, or label changes.
- Both original saves loaded using the full desktop game data. Normalized
  snapshots preserved the frame, current dialogue, and VM position. Both
  resumed to the next dialogue and exported/reloaded native `.dat` saves with
  companions. These outputs were written only to task Temp.
- ARM64 Android `assembleRelease` passed using AGP 8.5.2 through a temporary
  init script; the unrelated local AGP edit was preserved.
- APK signature verification passed and matched the previously installed
  certificate. APK SHA-256:
  `925397cfd897966bb0543d04e8dc90c67b79bd175f326ec943a249a0fd5bbdd4`.
- At the user's request, `adb install -r` succeeded on device
  `10DF1N05QN000H8` at 00:17:52 local time. Package `org.dc4engine` retains
  versionName `0.7.3` and versionCode `21`. The prior APK and the complete
  external save directory were copied into task Temp before installation.
- The original slots and global profile were preserved during APK installation.
  Interactive load/save verification in the installed Android app is left to
  the user's test.

## Imported prologue profile repair

After the user explicitly requested correction of the D.C. unlock, the complete
save directory was backed up again and the app was stopped to avoid an old
in-memory profile overwriting the repair. In this imported Plus Harmony profile,
flags 100-119 were changed from one to zero to match the supplied prologue
saves. Exactly 20 bytes changed. Settings, seen CG/music/scenes, read history,
slots, and sidecars were retained.

The existing engine already starts DC4PH progress at zero and restores the
global profile over stale slot completion flags. This correction therefore
needed a repair of the polluted profile, with no additional APK rebuild.

Validation:

- The actual repaired profile was loaded through `SystemMenu`; D.C. was absent
  from controls and could not be activated. It remained locked after loading
  each original slot, synchronizing progress, saving the profile, and reloading
  it into a new menu instance. Dialogue and VM position were preserved.
- The DC4PH `system_menu_test --progression` cases passed, including fresh and
  completed profiles, real ending prerequisites, restart, and stale saves.
- The repaired profile was atomically installed in the phone's save directory.
  The game was relaunched and its actual title screen was inspected: D.C. was
  absent, and END occupied the locked layout's final row.
- The whole save directory was pulled again after launch. The repaired profile
  persisted, and all nine other files matched the latest backup byte for byte.

Profile SHA-256 before repair:
`61fad633af01002884b09deda8cee6194057238e8ec6b7c65dc9380f04b1ce98`.

Profile SHA-256 after repair:
`fdcab7a583d35ac74b529256b13993487a98a548a192f1c5bc79f73842fdc80a`.

The desktop native-save round trips establish that the supplied scripts can
export saves after restoration. They do not establish the cause of a separate
historical "original PC scenario is unavailable" error on another app session.

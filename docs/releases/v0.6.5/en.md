[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.5 - DC4 Route Progression and Map Selection Fixes

Version 0.6.5 fixes route prerequisites in D.C.4 and D.C.4 Plus Harmony. New profiles no longer start with completed routes, while profiles with 100% completion keep all their access.

### Route prerequisites and the D.C. menu

- The D.C. menu entry stays hidden and cannot be activated until the original script's ending requirements are met.
- Arisu/Arisa routes and additional Sorane choices follow the ending progress saved by the game. Missing completion flags now default to zero.
- Loading a save or resetting configuration no longer forces every route open.

### Map selection

- On the four maps for February 8, 9, 11, and 12, the additional character, location marker, and choice stay hidden until the four initial heroine prerequisites are met.
- Locked maps use the original artwork with three choices. Once unlocked, the original artwork containing the extra marker appears together with the fourth choice.
- Unavailable choices cannot be triggered. Map scores and branching still follow the player's choices and the game scripts' rules.

### Saves and completed progress

- Ending progress applies across the profile and survives loading older saves, rewinding from the backlog, or resetting configuration. Local map scores still come from the loaded save.
- An older Android profile can no longer overwrite completed progress imported from PC data with zero values.
- Existing saves and stored completion flags are retained. Fully completed profiles remain fully unlocked.

Older profiles may already contain completion flags created by the previous bug. Those values cannot be distinguished from earned progress, so 0.6.5 does not automatically reset or repair them. To recover known partial progress, import the corresponding PC progress data into a fresh emulator profile after backing up your data.

### Validation and limitations

Testing covers 24 desktop test suites, 1,348 map-choice combinations across both games, ending requirements, and progression save/restore. Title screens and all four maps were inspected in locked and unlocked states for both games. Android JNI code passed 24 map-branching cases, four locked-progression cases, nine replay cases, and the CP932 space check on an Android device.

Visual checks use the desktop renderer with original game assets; Android JNI checks use isolated test scripts. This is not a complete playthrough of every route or testing on every device. These compatibility fixes apply specifically to DC4/DC4PH. The fixes from 0.6.4 remain available, and DC2DM support remains in beta.

See the [DC4 progression details](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.5/docs/dc4-progression.md) and [0.6.5 package verification](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.5/docs/release-qa-0.6.5.md).

### Downloads and installation

[Open the v0.6.5 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.5)

- `Arisa32-v0.6.5.apk`: release package for regular use.
- `Arisa32-v0.6.5-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Install the update without uninstalling the app to retain your data. Arisa32 0.6.4 can download the update inside the app; Android still handles installation confirmation.

Android 5.0+; arm64-v8a and armeabi-v7a. Game files and translation patches are provided separately.

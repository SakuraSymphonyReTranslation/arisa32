# DC4PH startup and music catalogue inspection — 2026-11-06

## Recording

Source: `C:/Users/adamb/Downloads/dc4phmenuwhyjump.mp4`.
All 1,076 frames were extracted and inspected in consecutive contact sheets,
with full-size checks at the important transitions. Duration: 36.140 seconds.
Frame timestamps come from the original variable-rate stream, not the JPEG
extraction's sequential numbering.

| Frame / time | Observation |
| --- | --- |
| 1–25 | DC4PH library path points at the Indonesian translation patch's AdvData. |
| 26 / 2.361 s | Launch overlay. |
| 28 / 2.411 s | Black game canvas; no title screen. |
| 110 / 4.833 s | Scenario message window appears. |
| 244–245 / about 8.8 s | Cursor mode is Off. |
| 377 / about 12.733 s | User confirms Title screen. |
| 472 / 15.532 s | Scenario message window returns. |
| 732 / 25.407 s | Ordinary DC4 opens its title screen. |
| 918 / 31.261 s | Cursor enabled later, in ordinary DC4. |

Evidence directory: `build/dc4ph-startup-video/`. This contains the complete
frame set, timestamps, contact sheets, device log and configuration backup.

## Cause and correction

The recording used `org.dc4engine.updateqa`. Its DC4PH base path was:

`/storage/emulated/0/Gamehub/D.C.4 Da Capo 4 Plus Harmony Indo Translation Patch/AdvData`

That partial directory has MES scripts, but lacks GRP/NAMES.DAT and VOICE.DAT.
The device log at 02:44:47 records:

```text
system menus unavailable (Invalid or missing version 7 GRP/NAMES.DAT); using in-game flow
in-game flow ready (dc4_kyo20190204a1.mes)
```

This fallback also explains why requesting Title repeated the scenario.
The normal application's library already used the complete directory:

`/storage/emulated/0/Gamehub/D.C.4 Plus Harmony/AdvData`

Its GRP/NAMES.DAT is 73,188 bytes. After backing up the QA library, only the
DC4PH `.path` property was changed to this complete directory. The translation
overlay, cursor preference, other settings and save files were retained.

The launcher now checks the NAMES.DAT version header for data containing
DC4 scenario scripts. Scans skip incomplete candidates; registration rejects
them before fingerprint matching can replace a complete entry; launch shows
a localized explanation and stays in the library. Translation overlays still
accept partial data, and classic game probing keeps its existing behavior.
The QA APK now has the Android launcher label `Arisa32 QA`.

## Validation and installation

`GameDataChecks`: **64 checks passed** on I2219, serial `10DF1N05QN000H8`.
Tests cover both modern MES versions, identical script fingerprints in a
complete base and partial patch, persisted path preservation, translation
registration, missing/wrong/truncated catalogue headers, lowercase filenames,
classic probing, EN/ID/JP launch errors and Cancel, native title startup and
the Title action. Data and the temporary library are isolated fixtures.
The device's QA launcher preferences were restored after instrumentation.

Both APK builds succeeded. Normal and QA packages were updated in place.
Normal APK: `build/dc4ph-startup-video/Arisa32-startup-debug.apk`, version
0.6.8 / code 15, SHA-256:

`89d35454c89ea3e34d99b50de15a3b412d29a2d87e1238cc558cccdae757d980`

The real DC4PH music menu was also observed rendering on the installed normal
application. Title startup and the Title action were asserted with native
synthetic fixtures; the music screenshots are not title-screen evidence.

## Music rows reported during verification

Compared the device's original music.mes, NAMES.DAT, music title artwork and
system-profile.dat for both games. Each menu's row IDs are contiguous.

| Game | Authored music rows | Unlocked rows in current profile | Blank/locked rows |
| --- | ---: | ---: | --- |
| DC4 | 42 | 42 | None |
| DC4PH | 43 | 40 | 5, 32, 33 |

DC4PH's three missing progress records correspond to:

- MUSIC/005: Sakura yang Melayang Lembut
- MUSIC/032: Hati yang Bersemangat
- MUSIC/033: Memainkan Melodi Hati

All three PCM files exist. All three titles exist in SYSTEM/MUSICTITLE.crx.
Their source rectangles were checked against music.mes, including the second
artwork column for row 33. The first blank visible row is number 5, matching
the missing music/005 progress record. `music_rows()` draws title chips only
when `music_unlocked()` finds the corresponding music in the profile.

DC4 also has a native system.dat on the device; DC4PH has an emulator profile
but no system.dat in the checked save directory. This records the present
state; it does not establish why those three songs were never marked heard.
Music data, progress and unlock rules were not changed during this inspection.

Evidence: `build/dc4ph-startup-video/music-data/catalogue-audit.json` and
`music-data/dc4ph/locked-title-evidence.png`, plus the extracted scripts and
read-only copies of device profiles.

The user subsequently confirmed that the DC4PH story is unfinished. The
locked-entry presentation was then corrected and both entire catalogues were
rendered and checked; see `music-locked-marker-validation.md` for the fix,
profile preservation checks and updated APKs.

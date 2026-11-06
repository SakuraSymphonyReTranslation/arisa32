# Locked Music Mode entries, 6 November 2026

DC4 and DC4PH now display `??????` for locked music entries. Previously,
the modern menu renderer omitted the title without drawing a replacement.
The marker uses the authored title position and the existing menu font.
No unlock, audio selection, cursor, save or scenario logic was changed.

The user's DC4PH story is unfinished. Its three locked entries are consistent
with that progress; the blank rows did not establish lost or corrupted saves.
The marker is an Arisa32 presentation fix. Exact equivalence to the PC engine's
locked-entry text has not been established from a PC runtime capture.

## Entire catalogue verification

`menu_music_test` renders every row in three states: fully locked, fully open,
and the profile copied from Android. It checks visible marker pixels, visible
title artwork, the matching profile state, disabled pointer interaction, every
row's BGM selection mapping, catalogue bounds, scrolling, repeat navigation
and playlist selection. Full unlocks exist only in test memory and are never
saved. This checks playback mappings, not listening to every audio file.

| Game | Rows rendered | Profile unlocked | Profile locked | Result |
| --- | ---: | ---: | --- | --- |
| DC4, Indonesian overlay | 42 | 42 | None | PASS |
| DC4PH | 43 | 40 | 5, 32, 33 | PASS |

The DC4PH `music.mes`, `NAMES.DAT` and `MUSICTITLE.crx` used on desktop match
the Android copies by SHA-256. DC4's catalogue and Indonesian music script
also match the Android copies. Neither checked DC4PH overlay supplies a
replacement music script or music-title sheet. Desktop checks explicitly
load the copied Android profile, without importing the PC's system.dat.

All eight DC4PH page captures and seven DC4 page captures were visually
inspected. The final overlapping DC4PH page includes row 43, Harmony. Rows
32 and 33 draw correctly across the title atlas's column boundary.

The existing synthetic `system_menu_test` passed. The music test contained
an obsolete assertion permitting saves during modern scene replay; it now
matches the existing replay policy tests and checks ordinary-story restoration.
No replay implementation was changed for this fix.

Commands, from the repository root after rebuilding `menu_music_test`:

```powershell
$env:MENU_REGRESSION_CAPTURES='build/music-locked-marker/dc4ph'
build/desktop/menu_music_test.exe 'H:/Games/D.C.4 Da Capo 4 Plus Harmony/Advdata' 43 android/app/src/main/assets/fonts/MSGOTHIC.TTC build/music-locked-marker/dc4ph-profile.dat
$env:MENU_REGRESSION_CAPTURES='build/music-locked-marker/dc4'
build/desktop/menu_music_test.exe G:/Games/DC4/AdvData 42 android/app/src/main/assets/fonts/MSGOTHIC.TTC build/music-locked-marker/dc4-profile.dat G:/Games/DC4/id_Data
```

Logs, copied profiles and contact sheets are in `build/music-locked-marker/`.
The `profile-all-pages.png` files contain the actual-profile renders;
`locked-page-*.ppm` and `unlocked-page-*.ppm` are isolated test states.

## Android delivery

Both arm64-v8a and armeabi-v7a builds succeeded. Both installed packages were
updated using `adb install -r` on I2219, without clearing application data.
The normal package's live Music Mode displayed the new marker for track 5:
`build/music-locked-marker/device-music-first.png`. It shows catalogue rows
2 through 7, despite the capture filename. Whole-list verification ran in
the shared desktop renderer; it was not repeated page by page on Android.

| APK | Package | SHA-256 |
| --- | --- | --- |
| `Arisa32-music-debug.apk` | `org.dc4engine` | `da7428c1041dadfc190ffdaa4542474bc2afe2bf8bd5cc9e797f6a8dc724b853` |
| `Arisa32-QA-music-debug.apk` | `org.dc4engine.updateqa` | `5a85646f07bfa38e25efc0612a85cdf8f6be4bf3bd377bfb444b6b2ff3562e0f` |

APKs are under `build/music-locked-marker/`. The normal package retains version
0.6.8/code 15; the QA override retains 0.6.3/code 10 and label Arisa32 QA.
Both copied profiles were byte-identical immediately after installation.
`profiles-before.json` and `profiles-after.json` record the music-progress
comparison around the subsequent device check. The intermediate DC4PH copy
contained additional unlocks. The user confirmed importing/restoring a save
during that check, so it is not a controlled before/after comparison of the APK.

Fresh reads using `exec-out cat` and `adb pull` show the original 40/43 DC4PH
music state again, with rows 5, 32 and 33 locked. `profiles-current.json` records
the latest comparison: both games' CG, music, scene and route-progress records
match their pre-install copies. Menu position, playback position and playlist
records can change with interaction. No save restoration or progress edits
were performed by the assistant during this verification.

## Scoped UI delivery gate

- Hard gate PASS: all 85 authored rows remain within their original bounds;
  locked rows have visible text and reject pointer activation. Android shows
  the marker inside the existing row. Both ABI builds and installations passed.
- Purpose gate PASS: the added text identifies locked content. Existing title
  positions, artwork, palette and menu text rendering are reused.
- Liveliness PASS: the existing catalogue is the design reference, with
  ENERGY 1 / RHYTHM 1 / MOTION 1 for this static marker change. No layout,
  decoration, focal point or animation is introduced.
- Craftsmanship PASS: the 42/43 counts come from actual scripts; profile
  comparisons, all-row render assertions, disabled-click checks and captures
  support the reported results. The production patch adds only a locked-row
  caption. The latest profile comparison matches the original progress; the
  intervening user save import is recorded above.

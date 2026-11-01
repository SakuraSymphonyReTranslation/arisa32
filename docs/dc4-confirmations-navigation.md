# DC4 confirmations and floating controls

The DC4 and DC4 Plus Harmony CONFIG menu already changed native confirmation
preferences, but the host's quick-action, Save/Load and system-menu dialogs
kept separate local skip flags. They now read the same per-game profile. A
confirmed Yes with “Don't ask again” updates that profile immediately; No
leaves it unchanged. Re-enabling the corresponding CONFIG switch makes the
prompt appear again, including after a previous checkbox selection.

## Native mapping

Both installed CONFIG scripts (DC4 AAB6 and DC4PH ABB6) use bank 93 with
0 meaning ask/On and 1 meaning skip/Off. The binding is scoped to those games
using their existing scenario-catalogue identification.

| CONFIG action | Bank 93 index | Host action |
| --- | --- | --- |
| yesno01 | 0 | Quit |
| yesno03 | 4 | Return to title |
| yesno04 | 7 | Save, overwrite, quick-save |
| yesno05 | 8 | Load and quick-load |
| yesno06 | 1 | Backlog scene jump |
| yesno07 | 13 | Scene replay |
| yesno08 | 6 | Finish scene replay |
| yesno09 | 9 and 10 | Reset settings |

The native All On/All Off controls and reset remain authoritative. The screen
mode confirmation (indices 2 and 3) is for the PC window/full-screen operation;
this change does not add that operation to Android. Host-added New Game and
Movie prompts have no native switch and retain their existing local policy.
DC2PC, DC2DM and unidentified games do not receive the DC4 mapping.

## Floating navigation

The shared Android setting is available under app Settings > Appearance and
inside the in-game three-dot menu. It defaults to Off and persists globally.
On hides the floating controls after four seconds. A left swipe hides them
manually even when the timer is Off. Tap the cyan mark at the left edge, or
swipe it right, to restore them. DC4/DC4PH hide their three-dot control;
DC2PC/DC2DM hide and restore both the dots and Back / Sidemenu together.

The marker is 6dp wide inside a 48 by 64dp touch target. That small area captures
restoration gestures while hidden; it does not forward them to the game.
Dragging the marker vertically moves it without revealing the controls. Its
height is saved globally as a fraction of the safe vertical travel, separately
from the visible controls, and is restored when opening another game. A
vertical gesture stays vertical even if the finger subsequently turns right;
cancelled or multi-finger gestures restore the previous height without saving.

The dots start 4dp from the safe top-left corner on every game launch, with
Back / Sidemenu 8dp below when present. Legacy `bubbleX`/`bubbleY` screen
fractions are no longer restored. Other drag directions can move the visible
controls for the current game session. Child layout callbacks reapply the
safe-corner offsets after Android padding/layout changes, avoiding an extra
translation by the parent's padding. Positioning respects system-bar/cutout
insets, and DC2PC still confines dragging to the left black margin when it has
enough room. Full-width game images can still extend behind the controls.

The timer pauses with the Activity/window lifecycle and defers while the
controls are being touched, a loading/dialog state is active, keyboard focus
is on a control, or accessibility touch exploration is enabled. The handle
is keyboard-focusable and restores focus to the three-dot control on use.
The current implementation uses no hide/show animation.

## Verification and limits

Both actual-data `system_menu_test <AdvData> --confirmations` runs exit 0.
They exercise original CONFIG switches, shared quick-action/SaveScreen policy,
checkbox pointer input, cancellation, per-action isolation, profile reload,
the menu's Quit dialog, reset, and an unrecognized profile. The complete
desktop regression run passes 22 of 22 suites.

The earlier Android validation used an isolated `org.dc4engine.updateqa` Activity on the
I2219/API 36 and passed 207 checks. The real floating controls ran in a synthetic 320 by 180dp game
area for all four profiles. Gestures are dispatched into that QA layout, with
a counter checking they do not reach the simulated game. This establishes
Android view/controller behavior; it does not represent a live playthrough
of all four games. The earlier 89-check DC2DM sidebar run remains separate.

Evidence is under `build/dc4-navigation-validation/`: native disassemblies,
`native-confirmations.png`, the two confirmation logs, `qa-build.log`,
`navigation-device.log`, and `navigation-checks/` captures. Final captures wait
for a rendered frame; an initial capture pass was taken too early and was
replaced. The 320dp/200% text review also found and corrected the long chooser
title. The proprietary reference images are local validation data only.

No installed game assets were modified or packaged. The QA package is stopped
after each run, and the ordinary APK is built without the QA init script.

The earlier local APK was
`build/dc4-navigation-validation/Arisa32-DC2-DC4-fixes-debug.apk`, 14,186,606
bytes, SHA-256 `a2a8cab36ddc43836016501fe3e3cdca6bf77041fdbe570e6580ba8c6a15d897`.
It is the ordinary `org.dc4engine` debug package, version 0.6.2/code 9,
minimum API 21, target API 36, with arm64-v8a and armeabi-v7a libraries.
Both engine libraries match the tested QA build and current stripped output.
APK v1/v2 signatures verify. DEX inspection confirms the runtime controls and
filter are included and instrumentation classes are absent. Full artifact and
source hashes are in `final-apk-metadata.json`. No release was published.

## Corner and movable-handle extension, 2026-11-01

The latest focused device run passes 431 assertions across DC4, DC4PH, DC2PC
and DC2DM. It starts with deliberately inset legacy coordinates and verifies
the actual controls at the safe top-left corner after first layout, changed
padding, and 280x180dp, 320x180dp, 480x240dp and full-screen layouts. Handle
tests cover downward movement, upper/lower bounds, cancellation, second-finger
cancellation, vertical-axis locking, show/hide retention, preference reload
in a recreated controller, and relative height after resizing to 360x260dp.
The earlier timer, visibility, keyboard, restoration and localized-setting
checks run in the same pass. No navigation gesture reaches the fake game.

The primary agent inspected full-screen DC2DM/DC4PH corner captures and the
moved DC2DM handle capture. These are synthetic Android view fixtures, not
live game screenshots. The test used one physical I2219/API 36 with several
layout sizes; it does not claim coverage of every phone/OEM overlay. Saved
height is global to this installation and adapts to available height; it does
not detect or reposition a Game Booster overlay itself. No full process-kill
or TalkBack run was performed for this extension. Native code was unchanged.

Evidence: `build/navigation-position-validation/qa-build.log`,
`navigation-device.log`, `navigation-checks/`, `final-build.log`,
`final-apk-badging.txt`, `final-apk-signature.txt`, and
`final-apk-metadata.json`. The final APK is
`build/navigation-position-validation/Arisa32-navigation-position-debug.apk`,
14,186,944 bytes, SHA-256
`ed2c5603806cb5d5dc2ac6fc989457033c6eb42496c512ed1f3bb78f395c859b`.
It remains `org.dc4engine` debug v0.6.2/code 9, minimum API 21/target 36,
arm64-v8a and armeabi-v7a. v1/v2 signatures verify; native libraries match the
tested QA APK, instrumentation classes and game assets are absent. Only the
QA application was installed for testing and stopped afterward. The ordinary
APK is a local deliverable; production was not replaced and nothing published.

# Fullscreen, COMU lifecycle and gear transition

Validated 2026-11-05. These changes follow v0.6.7 and are included in v0.6.8. The evidence below records the feature build before the version bump; see [0.6.8 package verification](release-qa-0.6.8.md) for the final release artifacts.

## Behavior

- Per-game **Full screen / Layar penuh** is available in game settings and the in-game menu. On hides the navigation bar; Off shows it. Existing entries default to Off. A system edge swipe can temporarily reveal navigation.
- Fit to Screen retains 1280x720 proportions; Stretch fills the available surface. Fullscreen resizes that surface and retains the selected screen mode. Touch coordinates, floating controls and movie transforms follow the surface.
- COMU history clears on a DC4/PH background commit. Slot preloading, normal narration and `:n` / `:r` flags do not clear it. An outgoing transition snapshot retains the phone until the transition reveals the next scene.
- DC4/PH screen-effect type 2 uses slot 22's original `SYSTEM/EFFCHANGESCREEN2` atlas. It expands and contracts around the native pivot without rotation. Existing flower effects remain supported.

## Native and recording evidence

The corrected recording is `20261105-0244-02.9424540.mp4`: 1280x746, 30 fps, 456 decoded frames. All frames were extracted. Transition frames 150 through 279 were inspected in consecutive contact sheets. Gear growth is visible around frames 168-183, with contraction around 230-242; intervening frames show the gear outside the screen. The recording contains repeated frames and jumps, so timing follows the native motion queue rather than fitting those capture gaps.

Local evidence is under `build/fullscreen-comu-gear/`: `frames.csv`, `frames/`, `gear-frames-*.jpg`, `DC4-gear.asm`, `PH-gear.asm`, and `PH-reset.asm`.

| Native behavior | DC4 | Plus Harmony |
|---|---|---|
| Background commit resets the scene | `0x42B192 -> 0x425DB0` | `0x42B002 -> 0x425C20` |
| Scene reset clears COMU timeline | `0x425F20 -> 0x4581F0 -> 0x4582F0` | `0x425D90 -> 0x457F90 -> 0x458090` |
| COMU drawing requires a nonempty timeline | `0x458260`, count at `+0x23C` | `0x458000`, count at `+0x23C` |
| Type 2 gear setup | `0x42FBB7` onward | Position at `0x42FA68`, followed by crop/pivot/scale setup |
| Queued scale animation | `0x4478E0` | `0x4476C0` |

Both executables specify position (280, 0), crop (0, 0, 732, 720), pivot (360, 355), and scale 1% to 500% to 550% to 0%. Growth and shrink each take `12 * parameter` frames, with a 30-frame middle phase. Total duration is 1300 ms for parameter 2 and 2500 ms for parameter 5. Ambient-effect preferences do not suppress this finite scripted transition.

In the Indonesian `dc4_mid20190421a.mes`, offsets relative to code start include the snapshot at `0x5F55`, `BG\BLACK` commit at `0x5F66`, and the reported narration at `0x60BA`. The command `0x28(13,0)` at `0x5F79` stops fairy lights; it is not the COMU dismissal. Gear assets load at `0x62DE` / `0x62EC`, with type 2 calls at `0x6312`, `0x6327` or `0x6330` according to the script branch.

## Verification

| Check | Result |
|---|---|
| Host preferences | PASS: 20 assertions for defaults, persistence, independence and malformed values. |
| Android screen checks | PASS: 440 assertions on iQOO Z9x, Android 16/API 36; `device.log`. |
| COMU regression | PASS: `comu-test.log`, 0 failures; both MES versions, preload versus commit, preserve flags, outgoing snapshot, empty timeline and save restoration. Existing padding, Sakuramusubi prose and font cases also run. |
| Effect regression | PASS: `effects-test.log`, 0 failures; both MES versions, gear phases/durations, rotation orientation, finite waits, skip, save restoration, missing asset and zero duration, plus existing flowers/masks/ambient effects. |
| Original assets | PASS: `original-effects.log`, 0 failures with DC4 and PH original effect atlases, native ambient saves and authored replay triggers. Gear captures are under `original-effects/DC4_DATA/` and `original-effects/DC4PH_DATA/`. |
| Scene regression | PASS: `scene-test.log`, Scene regressions passed. |
| QA build | PASS: `qa-build.log` and `qa-test-build.log`. |
| Production release build | PASS: `release-build.log`; arm64-v8a and armeabi-v7a APK. |
| Production installation and launch | PASS: installed on iQOO Z9x; `launch.log` reports `Status: ok` for `org.dc4engine/.Arisa32Activity`. Installed APK SHA256 matches the delivered file. |

Delivered APK: `E:/Games/Adv32roid/build/fullscreen-comu-gear/Arisa32-v0.6.7-fullscreen-comu-gear.apk`. Package remains `org.dc4engine`, version 0.6.7 (code 14). SHA256: `5508f2b9c6d2aaba41cdee9ccec962c54e4a69781284e3ddf298c8653f7bda31`, recorded in `apk-hash.log` and `installed-hash.log`.

The first device run exposed an early `Window.getInsetsController()` call before the decor existed; `applyFullscreen()` now obtains the decor first. The keyboard harness explicitly exits touch mode and waits for the dialog layout. Native fixtures now identify the DC4 profile before creating a runner, removing dependence on the output directory's name.

Device checks cover navigation visibility, Fit/Stretch geometry, center touch coordinates, persistence, focus regain, return to the library, On/Off/Cancel/Back and keyboard selection. Existing movie-transform checks run in the same suite. Eleven captures are in `captures/`; fullscreen EN/ID/JP dialogs were checked at 200% text size. Rapid test selections can leave a prior toast over the next capture.

Android 5-10 uses immersive system-UI flags; Android 11+ uses WindowInsetsController. Runtime device coverage is Android 16, not every supported Android version. This is not an exhaustive route playthrough or a claim of frame-for-frame capture timing equality.

## Antislop delivery gate

Scope is the new setting, with the established Arisa32 dialog palette, spacing and typography. ENERGY 1 / RHYTHM 1 / MOTION 1. Scripted game motion reproduces existing game content and is separate from settings UI motion.

- R-02 PASS: added labels contain no em dash.
- R-03 PASS: row bounds checks and EN/ID/JP captures show wrapping without horizontal clipping at 200% text.
- R-17 PASS: setting displays the stored boolean, with no marketing statistics.
- R-18 PASS: no testimonials or invented people.
- R-23 PASS: user requested the setting and gear support; gear uses the game's existing atlas.
- R-24 PASS: library and in-game entries open the selector in device tests.
- R-25 PASS: inherited text contrast, #E6EDF5 on #131B26 14.68:1, #8899AC 5.94:1, #38BDF8 8.08:1, white 17.32:1; prior screen-mode validation records the palette.
- R-26 PASS: On/Off apply and persist; Cancel and Back retain the value; tested on device.
- R-27 PASS: absent or malformed values default to Off; saving uses the existing confirmation behavior.
- R-28 PASS: no FAQ added.
- R-32 PASS: native list focus and keyboard selection verified by the 440-check suite.
- R-33 PASS: feature is implemented in source, with no runtime source rewriting.
- R-34 PASS: existing fixed palette retained; no theme setting added.
- R-35 PASS: QA app built and run; all added controls exercised in the device suite.
- R-36 PASS: no security or performance claims added to the UI.
- R-37 PASS: existing Arisa32 dialog direction chosen before edits.
- R-38 PASS: labels reflect the implemented setting; fixtures remain test-only.
- R-01 PASS: inherited solid modal background, no gradient or glow.
- R-04 PASS: text choices, no decorative icons added.
- R-06 PASS: inherited native type and system scaling retained.
- R-07 PASS: no decorative grid.
- R-08 PASS: no decorative arrows.
- R-09 PASS: no badges.
- R-10 PASS: opaque native dialog, no glass treatment.
- R-12 PASS: inherited modal elevation only.
- R-13 PASS: no glow.
- R-14 PASS: two radio rows compare mutually exclusive choices.
- R-19 PASS: no new settings animation; gear motion follows authored script instructions.
- R-22 PASS: no generated illustrations.
- Dials PASS: ENERGY 1 / RHYTHM 1 / MOTION 1 declared for settings UI.
- Dial consistency PASS: existing list and radio selector, without added animation.
- Focal point PASS: setting title precedes the choices.
- Whitespace PASS: inherited row padding separates choices and tap targets.
- Accent PASS: existing cyan marks selection and actions.
- Identity PASS: existing Arisa32 caption and palette retained in captures.
- Design Read PASS: established settings presentation chosen before implementation.
- C-1 PASS: selector matches existing settings and the two exclusive states.
- C-2 PASS: entry, choices, cancellation and keyboard activation exercised.
- C-3 PASS: addition serves the requested fullscreen control.
- C-4 PASS: covered Fit/Stretch, EN/ID/JP, large text, focus, cancellation and keyboard; device coverage is stated above.
- C-5 PASS: counts and results derive from recorded tests; coverage limits disclosed.
- R-05 PASS: existing settings list extended without unrelated templates.
- R-11 PASS: inherited dialog and radio shapes retained.
- R-15 PASS: labels describe navigation-bar visibility; Cancel names its action.
- R-16 PASS: no marketing buzzwords.
- R-20 PASS: matches the app's existing settings language.
- R-21 PASS: existing app palette retained, no theme request deferred.
- R-29 PASS: existing foreground/background and cyan accent retained.
- R-30 PASS: uses the repository's native dialogs.
- R-31 PASS: visual choices preserve existing settings consistency; new code comments record native protocol constraints.

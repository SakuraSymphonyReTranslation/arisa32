# Optional cursor mode

Date: 2026-11-06. The user requested a Kirikiroid2-style cursor option with a disabled default.

## Behavior

- The per-game **Cursor mode / Mode kursor / カーソルモード** setting is available in game management and the in-game dots menu. Changes apply immediately.
- New entries and libraries without `cursorEnabled` default to Off. Only the explicit stored value `true` enables it.
- With On selected, sliding a finger moves the cursor relative to its current position. Lifting after movement does not click. A stationary tap clicks at the cursor position.
- Holding for Android's long-press interval presses the game pointer. Sliding then drags it; lifting releases it. Small finger jitter stays a tap.
- The cursor starts at the game center, stays inside the 1280 by 720 game canvas, and scales movement through the actual Fit or Stretch viewport. Its tip identifies the click coordinate.
- Four seconds without input hides the cursor. Touching the game reveals it. A held gesture keeps it visible.
- Extra fingers, cancellation, viewport changes, disabling the mode, Back, and focus loss cancel the current cursor gesture.
- The cursor hides during loading, movies, paused rendering and Android dialogs. Focus callbacks use the received focus value, avoiding stale `hasWindowFocus()` state during callback dispatch.
- Android controls, including the dots menu, edge handle and Back to gallery, retain direct touch.

This implements the requested relative pointer interaction. Exact gesture parity with every Kirikiroid2 version was not evaluated. The existing game engine determines which native controls support dragging and hover feedback.

## Implementation

`CursorOverlay` owns gesture state, delayed callbacks and drawing on the UI thread. It sends game-coordinate events through the Activity's existing render-thread queue. It does not receive touch events itself or add a focus stop. `GameLibrary` persists the preference per game. No native engine changes were needed for this feature.

## Validation

- Java persistence harness: **11 checks passed**, including missing/malformed preferences, per-game isolation, On/Off round trips and preserved screen mode.
- Android `cursorOnly`: **209 checks passed** on device `10DF1N05QN000H8` using the isolated QA application.
- Android `screenOnly`: **440 checks passed** for viewport geometry, display settings, direct input and fullscreen behavior.
- Android `galleryOnly`: **762 checks passed** for the existing CG preview behavior after adding cursor mode.
- Both QA and normal debug APKs built successfully for arm64-v8a and armeabi-v7a.

The cursor checks include landscape, portrait and small viewport geometry; movement versus click; jitter; long press and drag; bounds; cancellation; extra-finger actions; delayed callbacks; runtime toggles; idle hiding; settings persistence; and real dialog focus loss/restoration. Movie visibility is checked by exercising the Activity's movie-state gate, not by playing a video in this harness.

Real MotionEvents open native CG previews in synthetic DC4 and DC4PH games with the mode off and on. A tap away from a thumbnail selects the thumbnail under the cursor. Moving the cursor leaves the rendered CG unchanged, clicking advances it, and direct Android Back to gallery and edge-handle taps still work. Reopening the game applies the saved On state; disabling it restores direct touch immediately.

Settings checks cover EN/ID/JP at 200% font scale, On/Off, Cancel, Back and Tab/Enter selection. The Indonesian selector is also exercised at 380dp width. Native scrolling keeps long options reachable. Screenshots in `build/cursor-mode/device-final/` were visually inspected, including `id-keyboard-200.png`, `jp-keyboard-200.png`, `stretch-cg.png` and `fit-hidden.png`.

## Design direction and delivery gate

The direction follows the existing Arisa32 navy settings, cyan controls and Android text scaling. The requested pointer uses a white fill and black outline for visibility over changing CG content; cyan marks its pressed state. The pointer mirrors near edges to keep its full shape visible without moving its tip. ENERGY 1 / RHYTHM 1 / MOTION 1: the game remains the focal point and motion follows the player's input.

Hard gate:

- R-02: PASS. New labels and help use plain action descriptions without em dashes.
- R-03: PASS. Settings wrap and scroll at 200% text and 380dp width; gesture geometry also covers portrait and small viewports.
- R-17, R-18, R-36, R-38: PASS. Test counts are observed results; colored CG fixtures are explicitly synthetic. No testimonials or performance claims were added.
- R-23: PASS. The user requested the cursor and option. The pointer is drawn in code; the existing navigation structure is reused.
- R-24, R-26: PASS. The new entry opens a working selector. Both choices, Cancel and Back were exercised.
- R-25: PASS. The installed contrast checker reports dialog text `E2E8F0` on `131B26` at 14.05:1; help `8899AC` at 5.94:1; cyan `38BDF8` at 8.08:1. White/black pointer contrast is 21:1 and cyan/black is 9.80:1.
- R-27, R-28: PASS. The feature adds no data-loading or FAQ surface. Pointer visibility follows existing loading, dialog and movie states.
- R-32: PASS. Existing native radio controls provide keyboard focus and activation. The pointer overlay is nonfocusable and excluded from the accessibility tree.
- R-33: PASS. Changes are ordinary source edits.
- R-34: PASS. The existing app theme is retained.
- R-35: PASS. Both APK variants were built; settings and in-game input were exercised on the device, with screenshots inspected.
- R-37: PASS. Existing-app direction and the three dials were declared before edits.

Purpose gate:

- R-01, R-07, R-09, R-10, R-12, R-13, R-14, R-22: PASS. No gradients, background patterns, badges, blur, shadows, glows, cards or illustrations were added.
- R-04, R-08: PASS. The arrow identifies the actual input point; it is not button decoration.
- R-06: PASS. Settings reuse the app typeface and Android font scaling.
- R-19: PASS. Cursor movement tracks input; idle hiding clears the CG view. No decorative animation was added.

Liveliness:

- PASS. The declared dials match the result. The CG remains central, existing settings spacing separates each choice, cyan marks controls and pressed state, and the existing navy dialog motif is preserved.

Craftsmanship and quality locks:

- C-1 through C-5: PASS within the documented test scope. Placement, color and gestures have explicit purposes, every new setting works, no filler UI was added, lifecycle/viewport states were checked, and results distinguish simulated state checks from actual game input.
- R-05, R-11, R-15, R-16, R-20, R-21, R-29, R-30, R-31: PASS. Existing layout, radius, palette and identity are retained. Labels describe the selected mode and gestures. No marketing language or external product layout was copied. The two code comments explain threading and edge mirroring.

## APK

`build/cursor-mode/Arisa32-cursor-debug.apk`, package `org.dc4engine`, 14,261,609 bytes.

SHA-256: `ee9d5a2e1ae91a7ca6c3f1c7d9b5bfc1cae117ef015793763a54e63401bf7566`.

Installed with `adb install -r` on device `10DF1N05QN000H8`; the command returned `Success`. Package inspection confirmed version 0.6.8, version code 15. Starting `org.dc4engine/.Arisa32Activity` returned `Status: ok`. Installation preserved app data and did not enable cursor mode.

This is a local debug build. No release was published.

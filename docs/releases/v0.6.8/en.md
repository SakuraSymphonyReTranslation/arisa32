[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.8 - Full Screen, COMU, and Gear Transitions

Version 0.6.8 adds an Android fullscreen toggle, fixes phonechat/COMU remaining after scene changes, and restores the expanding and shrinking gear transition in D.C.4 and D.C.4 Plus Harmony.

### Full screen

- Full screen is available in each game's settings and the in-game menu. On hides the Android navigation bar; Off shows it again. Swipe from a screen edge to reveal navigation temporarily.
- The setting persists per game and applies immediately during play. Games without an existing preference default to Off.
- Fit to Screen preserves image proportions, while Stretch fills the available display area. Both follow fullscreen surface changes, including touch coordinates, floating control positions, and video presentation.
- Fullscreen mode reapplies when the app regains focus or resumes. Returning to the game library shows the navigation bar again.

### DC4 and DC4 Plus Harmony phonechat / COMU

- The phone frame and COMU history clear when the script applies the next background. Phonechat no longer remains over narration after the conversation ends.
- The outgoing scene image retains the phone during the transition until the next scene appears. Background preloading, ordinary narration, and text flags do not dismiss COMU early.
- The fix follows script commands and works alongside the Indonesian patch's manual spacing and Sakuramusubi formatting. The font and COMU text layout behavior from 0.6.7 remains available.

### Gear transitions

- Supports the expanding and shrinking gear effect at the points specified by DC4/DC4PH scripts, using the original game assets.
- Position, scaling pivot, scale phases, and duration follow the PC engine. The transition segment of the reference recording was examined frame by frame and checked against native commands and scripts.
- The effect supports script waits, skipping, and saved-state restoration. The Visual effect setting for sakura and floating lights does not disable gear transitions.

### Previous fixes

All 0.6.7 fixes remain included: FXAA on Android 5+, MS Gothic and custom dialogue font sizes, more readable Sakuramusubi COMU text, manual patch spacing, sakura/floating lights, voice replay references, and sprite priority. See the [0.6.7 notes](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.7/docs/releases/v0.6.7/en.md) for details.

### Testing and limitations

Feature testing passed 440 screen checks on an iQOO Z9x running Android 16/API 36 and 20 host preference checks. COMU, effect, and scene regressions also passed, including both games' original effect assets, skipping, and save restoration. Fullscreen dialogs were checked in Indonesian, English, and Japanese at 200% text size.

Android 5 through 10 uses immersive system-UI flags; Android 11 and later uses WindowInsetsController. Direct device testing used Android 16; testing does not cover every Android version, route, or patch. The recording contains repeated frames and jumps, so gear timing follows the PC engine. DC2 Dearest Marriage support remains beta. Game files, translation patches, and custom fonts are provided separately.

See the [0.6.8 package verification](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.8/docs/release-qa-0.6.8.md) and [feature validation](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.8/docs/fullscreen-comu-gear-validation.md).

### Downloads and installation

[Open the v0.6.8 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.8)

- `Arisa32-v0.6.8.apk`: release package for regular use.
- `Arisa32-v0.6.8-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Install the update without uninstalling the app to retain data. In-app updates are available since 0.6.4; Android still requests installation confirmation. Android 5.0+; arm64-v8a and armeabi-v7a.

[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.6.6 - Larger DC4 Text and a Translucent Control Handle

Version 0.6.6 enlarges dialogue text and character names in D.C.4 and D.C.4 Plus Harmony. The blue edge handle also becomes less prominent while Arisa32 controls are hidden, reducing its visual distraction during play.

### D.C.4 and Plus Harmony text

- The default Latin dialogue size, including Indonesian and English, increases from 20 to 22 pixels on the game canvas. Character names increase from 18 to 22 pixels.
- The Android dialogue window uses MS Gothic to bring letter shapes and spacing closer to the PC version. Menu fonts retain their previous settings.
- Line wrapping, progressive text reveal, and text restored from saves use consistent sizes and spacing. Scene checks cover three-line dialogue and name placement within the nameplate.

### Control handle

- The blue handle on the left edge uses 40% opacity while idle. It stays visible and returns to full opacity when touched or focused with a keyboard.
- Tap or swipe right to reveal the controls. Drag up or down to move the handle; its position remains saved.
- The touch target remains 48 x 64 dp. Four-second auto-hide, gestures to hide the controls, and keyboard navigation remain available.

### Validation and limitations

Text rendering and scene regression tests passed. Visual checks used original assets from Japanese DC4 and Indonesian DC4PH, including name size and three-line dialogue boundaries. Testing on an I2219 running Android 16 passed 431 navigation checks across DC4, DC4PH, DC2 Plus Communication, and DC2 Dearest Marriage. Device captures also confirm a subdued idle handle and a bright focus state.

Android font rasterization can still differ from PC. These checks cover the recorded scenes and cases, not every route, patch, or device. The text size changes apply specifically to DC4/DC4PH; the control handle applies to profiles using floating controls. The route progression and map fixes from 0.6.5 remain available, and DC2DM support remains in beta.

See the [0.6.6 package verification](https://github.com/SakuraSymphonyReTranslation/arisa32/blob/v0.6.6/docs/release-qa-0.6.6.md).

### Downloads and installation

[Open the v0.6.6 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.6.6)

- `Arisa32-v0.6.6.apk`: release package for regular use.
- `Arisa32-v0.6.6-debug.apk`: debug package for testing and diagnosis.
- `SHA256SUMS.txt`: checksums for both APKs.

Install the update without uninstalling the app to retain your data. Arisa32 0.6.4 or newer can download the update inside the app; Android still handles installation confirmation.

Android 5.0+; arm64-v8a and armeabi-v7a. Game files and translation patches are provided separately.

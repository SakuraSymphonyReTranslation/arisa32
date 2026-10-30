[Bahasa Indonesia](id.md) | **English**

# Arisa32 v0.2.0 - CIRCUS adv32 Runtime for the Da Capo and Dal Segno Series

> [!IMPORTANT]
> ### Generated entirely by AI (LLM)
> All source code, system architecture, adv32 MES opcode reverse engineering, asset decoders, Android/JNI interfaces, and repository documentation were created by a Large Language Model (LLM).
>
> ### Game compatibility: Da Capo and Dal Segno (CIRCUS adv32)
> This runtime supports visual novels powered by CIRCUS adv32, with a focus on the Da Capo and Dal Segno series.

## Release notes: v0.2.0

### Expanded game catalog: 21 official titles

Game Library recognizes 21 Da Capo and Dal Segno titles through automatic MES script detection:

- **Da Capo 4 series**:
  - `D.C.4 ~Da Capo 4~`
  - `D.C.4 ~Da Capo 4~ Plus Harmony` (recognized separately through `dc4_asu*` route scripts)
- **Da Capo III series**:
  - `D.C. III ~Da Capo III~`
  - `D.C. III R ~Da Capo III R~`
  - `D.C. III P.P. ~Da Capo III Platinum Partner~`
  - `D.C. III With You ~Da Capo III With You~`
  - `D.C. III Dream Days ~Da Capo III Dream Days~`
- **Da Capo II series**:
  - `D.C. II ~Da Capo II~`
  - `D.C. II Spring Celebration ~Da Capo II Spring Celebration~`
  - `D.C. II P.C. ~Da Capo II Plus Communication~`
  - `D.C. II To You ~Da Capo II To You~`
  - `D.C. II Fall in Love ~Da Capo II Fall in Love~`
  - `D.C. II Dearest Marriage ~Da Capo II Dearest Marriage~`
- **Original Da Capo series**:
  - `D.C. ~Da Capo~`
  - `D.C.P.C. ~Da Capo Plus Communication~`
  - `D.C. White Season ~Da Capo White Season~`
  - `D.C. Summer Vacation ~Da Capo Summer Vacation~`
  - `D.C.A.S. ~Da Capo After Seasons~`
  - `D.C. Dream X'mas ~Da Capo Dream X'mas~`
- **Dal Segno series**:
  - `D.S. -Dal Segno-`
  - `D.S. i.F. -Dal Segno- in Future`

Edition variants are grouped under their main title. Non-adv32 games, such as D.C. 4 Fortunate Departures, use a different engine and are outside this runtime's scope.

### Direct playback from MicroSD and USB OTG

- Runs games directly from external MicroSD cards and USB OTG drives (`/storage/XXXX-XXXX`).
- Game files stay on external storage without being copied into the app's internal storage.
- Includes automatic storage-volume scanning and an in-app folder browser.

### Bundled Japanese font (`MSGOTHIC.TTC`)

- Installs the default `MSGOTHIC.TTC` font from the APK, so users do not need to copy a font manually for CIRCUS dialogue text.
- Users can still select the Android system font or import a custom `.ttf`, `.otf`, or `.ttc` font.

### Library management and duplicate prevention

- Game cards provide **LAUNCH**, **MANAGE**, and **REMOVE** actions.
- Removing a game copied to internal storage offers an option to delete the copy and free space.
- Removing an external-storage game only removes its library entry; original files on the USB drive/MicroSD card and save data are preserved.
- Canonical directory paths prevent duplicate registrations.

### Runtime architecture and JNI

- Renames the C++/JNI runtime module and Activity to `arisa32_jni.cpp` and `Arisa32Activity`.
- Retains `Dc4Activity` compatibility so older shortcuts and intents continue to work.
- Supports `arm64-v8a` and `armeabi-v7a`, with Android 16 (API 36) and 16 KB page-size readiness.

### Download

[Open the v0.2.0 download page](https://github.com/SakuraSymphonyReTranslation/arisa32/releases/tag/v0.2.0)

- `Arisa32-v0.2.0-debug.apk`: standalone debug APK for Android 5.0+ (ARM64 / ARMv7).

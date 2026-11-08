# Android library and save-archive checks

## Optional cursor mode

The per-game cursor option defaults to Off for new games and older libraries.
Host-side persistence checks:

```powershell
javac -encoding UTF-8 -cp "$env:LOCALAPPDATA/Android/Sdk/platforms/android-36/android.jar" -d build/cursor-mode/preferences `
  android/app/src/main/java/org/dc4engine/GameLibrary.java `
  (Get-ChildItem android/app/src/main/java/org/dc4engine/profile/*.java).FullName `
  tests/android/CursorPreferencesChecks.java
java -cp build/cursor-mode/preferences org.dc4engine.CursorPreferencesChecks
```

Build and install the isolated QA packages as described below, then run:

```powershell
adb shell am instrument -w -e cursorOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

This checks relative motion, tap versus movement, touch slop, hold-and-drag,
viewport bounds and scaling, cancellation, multiple fingers, runtime toggles,
focus loss and restoration, movie visibility, and four-second auto-hide.
The real Activity opens synthetic DC4 and DC4PH gallery fixtures through both
direct touch and cursor clicks. Moving the cursor must leave the CG unchanged;
clicking advances it. Android navigation controls remain directly tappable.
Settings are exercised in EN/ID/JP at 200% text, including a 380dp Indonesian
dialog, Cancel, Back and keyboard selection. Screenshots are saved under
`/sdcard/Android/data/org.dc4engine.updateqa/files/cursor-checks/`.
See `docs/cursor-mode-validation.md` for scope and results.

## Release descriptions and update dialog

Run the Java 17 text checks from the repository root:

```powershell
javac -encoding UTF-8 -d build/release-notes-check android/app/src/main/java/org/dc4engine/ReleaseNotes.java tests/android/ReleaseNotesChecks.java
java -cp build/release-notes-check org.dc4engine.ReleaseNotesChecks docs/releases
```

The checks cover app-language selection while the default locale is Japanese,
safe tag URLs, Markdown cleanup, code/filenames, both language selectors, empty
notes, and every published document's final line beyond the old 500-character limit.

For the device UI checks, build an isolated QA application from `android/`:

```powershell
.\gradlew.bat -I ../tests/android/update-qa.init.gradle :app:assembleDebug :app:assembleDebugAndroidTest
$appMeta = Get-Content app/build/outputs/apk/debug/output-metadata.json | ConvertFrom-Json
$testMeta = Get-Content app/build/outputs/apk/androidTest/debug/output-metadata.json | ConvertFrom-Json
if ($appMeta.applicationId -ne 'org.dc4engine.updateqa' -or $testMeta.applicationId -ne 'org.dc4engine.updateqa.test') { throw 'Unexpected QA package IDs' }
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell am instrument -w org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

Keep the test device unlocked. The harness opens the real Activity in its own
application package and intercepts the storage-permission prompt and external
release intents. It verifies that Update now stays inside Arisa32, including a
release with no APK. It tests the actual v0.6.3 Indonesian and English notes
at widths of 540dp and 380dp, with font scales of 100% and 200%. Checks include
touch scrolling, the final line, visible actions, Back, and keyboard activation.
Screenshots are saved under
`/sdcard/Android/data/org.dc4engine.updateqa/files/update-dialog-checks/`.

After publishing the documents, verify the same HTTP reader used by the Activity:

```powershell
adb shell am instrument -w -e networkOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

This compares both downloaded translations with the fixtures and checks the
missing-translation HTTP error. A notes failure must not block the update actions.
Uninstall only `org.dc4engine.updateqa.test` and `org.dc4engine.updateqa` after QA.
A normal build without the init script keeps the production package ID.

## In-app download and installation

Host-side download checks (Java 17, no network):

```powershell
javac -encoding UTF-8 -d build/update-download-check android/app/src/main/java/org/dc4engine/UpdateDownload.java tests/android/UpdateDownloadChecks.java
java -cp build/update-download-check org.dc4engine.UpdateDownloadChecks
```

The 48 cases cover known/unknown lengths, byte progress, checksum verification,
absolute/relative redirects, redirect loops, HTTPS downgrades, HTTP failures,
truncation, excess bytes, incorrect metadata, oversize payloads and cancellation.
They also cover the scoped GitHub API fallback, retry limits, network failures,
progress resets, and cancellation during retry delays.
Every failure must remove the partial file and close its connection.

For device tests, prepare these APK fixtures under the QA application's
external-files `update-fixtures/` directory:

- `candidate.apk`: build with `-I ../tests/android/update-qa.init.gradle -PqaUpdate`;
  it uses the QA package ID, version 0.6.4 / code 11, and the normal debug signer.
- `wrong-package.apk`: a normal production-ID build; never install this fixture.
- `wrong-signer.apk`: re-sign a copy of the candidate with a disposable test key;
  never install this fixture.

Build and install the QA application and instrumentation without `-PqaUpdate`
after saving the candidate, so the installed test application is code 10.
The init script pins that baseline to 0.6.3 / code 10 and the candidate to
0.6.4 / code 11 independently of the production release version.
Keep installation from this QA application disabled initially. Run:

```powershell
adb shell am instrument -w -e installerOnly true -e installMode stage org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

This checks package/version/signer rejection, real package staging, failed
downloads, retry, cancellation, cleanup, EN/ID/JP progress at 100%/200% text,
visible controls at 540dp/380dp widths, UI reattachment during download,
the permission-settings intent, and rejection of unrelated installer callbacks.
It leaves one complete staged session for a restart/installation check.
Without `installMode stage`, the test also cancels that session and checks cleanup.

With installation still disabled, `-e installMode recover` checks recovery
across process restart and cancels the recovered session. Alternatively, allow
installation through Android's settings for **the QA application only** and
use `-e installMode cancelInstall` to resume the staged session, open the actual
Android confirmation and cancel it. Some OEMs keep a separate permission state;
`appops set` alone may not grant this permission.

Finally, with installation allowed, `-e installMode installNew` downloads and
installs the candidate. Replacing the package terminates the instrumentation;
check success externally using `adb shell dumpsys package org.dc4engine.updateqa`
(version code 11 / version name 0.6.4). Reopen QA and verify that its private
`cache/arisa-update.apk.part` and `app-update` session entries are absent.
Screenshots go into the QA external-files `update-install-checks/` directory.
Delete the QA fixtures or uninstall only the QA packages when done. No public
Downloads directory is used by the implementation.

With the same isolated QA packages, run the DC2DM sidebar navigation checks:

```powershell
adb shell am force-stop org.dc4engine.updateqa
adb shell am start -W -n org.dc4engine.updateqa/org.dc4engine.Arisa32Activity
adb shell am instrument -w -e sidebarOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

This launches generated DC2DM fixtures through the real Activity, then taps
Back / Sidemenu, presses system Back, and uses the Show/Hide sidebar settings.
The DC2DM navigation buttons must remain inside the visible system-bar safe area.
Navigation controls respect system-bar padding and show keyboard focus for all
profiles. DC2PC retains its existing left black-margin placement.
Ordinary story navigation must open and close the sidebar without advancing
dialogue, including Tab focus and Enter activation. Replay flags 1 and 2 must keep it closed
and reject both Save and Load screen requests. Fixtures remain in the QA
app's private files; no installed game data is used. Screenshots are saved under
`/sdcard/Android/data/org.dc4engine.updateqa/files/sidebar-navigation-checks/`.

The global texture smoothing checks use the same isolated QA packages:

```powershell
adb shell am instrument -w -e graphicsOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

These verify nearest and bilinear pixels, software fallback, and real FXAA GPU
output on Android 13 or later using a headless `HardwareRenderer`. They also
exercise touch and keyboard selection, immediate changes, persistence after
relaunch, and English/Indonesian/Japanese labels at 200% font size. Screenshots
and GPU output are saved under
`/sdcard/Android/data/org.dc4engine.updateqa/files/frame-filter-checks/`.
See `docs/texture-smoothing-validation.md` for filter scope and compatibility.

The floating controls have a focused device check with the same QA packages:

```powershell
adb shell am instrument -w -e navigationOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

It exercises the real Activity controls for DC4, DC4PH, DC2PC and DC2DM in
synthetic game areas from 280 by 180dp to full screen. Checks cover initial
safe-corner placement despite legacy saved coordinates, padding/size changes,
vertical handle dragging, bounds, persistence across controller recreation,
axis locking and cancelled/multitouch drags, four-second auto-hide, shared
visibility of the dots and Back / Sidemenu, left-swipe hiding, handle tap/right
swipe/keyboard restoration, temporary control dragging, paused timers,
system-bar padding, and EN/ID/JP On/Off settings at 200% text. Pointer events
are dispatched only into the QA layout; game-touch counters reject accidental
forwarding. This fixture does not launch native game scenarios. Screenshots
are saved in `navigation-checks/` beneath the QA package's external files.
Stop only the QA package after testing. See
`docs/dc4-confirmations-navigation.md` for native confirmation validation.

Standalone export checks (Java 11+ and Android SDK; no device required):

```powershell
javac -encoding UTF-8 -cp "$env:LOCALAPPDATA/Android/Sdk/platforms/android-36/android.jar" -d build/java-export-check `
  android/app/src/main/java/org/dc4engine/SaveArchive.java `
  tests/android/ExportChecks.java
java -cp build/java-export-check org.dc4engine.ExportChecks
```

These check both companion modes across DC2PC/DC4/Plus Harmony filenames,
mixed-case extensions, retained save/profile/preview bytes, source preservation,
round trips into fresh directories, progress and failed/empty exports.

Profile checks need a Java 11+ JDK and the Android SDK's `android.jar` on the
compile classpath; they do not start an Android runtime:

```powershell
javac -encoding UTF-8 -cp "$env:LOCALAPPDATA/Android/Sdk/platforms/android-36/android.jar" -d build/java-check `
  android/app/src/main/java/org/dc4engine/GameLibrary.java `
  (Get-ChildItem android/app/src/main/java/org/dc4engine/profile/*.java).FullName `
  tests/android/ProfileChecks.java
java -cp build/java-check org.dc4engine.ProfileChecks
```

They verify MES-header detection, profile precedence, neighboring dialect
isolation, library titles, DC2PC SaveData discovery/defaults, native-save
fallback and batched slot lookup without repeated directory scans, reserved
latest-copy records, DC2PC's sparse native page numbering, scenario identities
in old/native saves, executable title tables and native-stem thumbnails.

The older archive harness also needs the Android SDK's `android.jar` on the
compile classpath because `SaveArchive` now contains document-provider code.
Include the profile sources when compiling it:

```sh
javac -encoding UTF-8 -cp "$ANDROID_HOME/platforms/android-36/android.jar" -d build/java-check \
  android/app/src/main/java/org/dc4engine/GameLibrary.java \
  android/app/src/main/java/org/dc4engine/SaveArchive.java \
  android/app/src/main/java/org/dc4engine/profile/*.java \
  tests/android/LibraryChecks.java
java -cp build/java-check org.dc4engine.LibraryChecks
```

The harness creates isolated temporary data, then checks portable archive round
trips, rejection of malicious or damaged ZIP/save/profile records, preservation
of existing data on failures and old archives, interrupted-import recovery,
and catalogue fingerprint/persistence behavior. It removes only its temporary
root after execution. Its archive expectations describe the older strict
identity-validation API; the current archive importer skips the manifest, so
the harness currently fails at `wrong game rejected`.

The generated save records have valid emulator envelopes/checksums; complete
VM/resource restoration is tested by the C++ `scene_test` target. Android UI,
document-provider permissions, fonts and lifecycle still require device tests.

## Replay JNI regression probe

After building the debug APK, run this focused test on a connected arm64 Android
device (API 26 or newer):

```powershell
python tests/android/run_replay_probe.py
```

The runner accepts `--apk`, `--jdk` and `--sdk` paths. It compiles a small Java
fixture host, extracts the APK's actual arm64 libraries, and uses `app_process`
without installing or opening an app. All generated MES fixtures, saves,
profiles and checkpoints stay in a unique `/data/local/tmp/arisa32-replay-probe-*`
directory. It does not read or change installed game data. Local evidence,
including the tested library SHA-256 and `result.log`, is retained under
`build/dc4-replay-validation/jni-probe-*`.

The nine cases cover both DC4 MES versions (`AAB6` and `ABB6`) with replay,
ordinary-story and unidentified-game fixtures, plus DC2DM (`9D72`) with replay
flags 1 and 2 and ordinary-story flag 0. They verify that all four replay
Save/Load toolbar taps return no message and leave dialogue unchanged, native
Save/Load and their screens reject replay, and backgrounding preserves existing
checkpoint bytes. DC2DM replay also blocks the native sidebar through its menu
button, direct setter and Back entry points, as well as quick-save/quick-load
confirmations. Ordinary stories and other modern games still dispatch the four
normal actions and complete a save/load round trip. The fixtures use generated
scripts and minimal synthetic artwork, with no proprietary assets or audio;
they do not establish visual Android UI or Activity lifecycle behavior.

The runner also checks a generated English dialogue whose MES spaces use byte
`0x5F`. Android's `windows-31j` decoder maps the resulting `0x7F` to U+001A;
the engine must normalize those spaces before calling Java.

For a negative regression check, `--libraries <directory>` runs against a
previously extracted set of arm64 shared libraries. The library before the
disabled-tap fix fails with `touch :qs returned: Not available on Android:
:disabled`.

## DC4 display settings

Build and install the isolated QA application and instrumentation with
`update-qa.init.gradle`, then run:

```powershell
adb shell am instrument -w -e displayOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

The checks open the actual per-game settings for DC4 and DC4PH. They exercise
Visual effect On/Off (sakura and floating lights), custom font sizes 18 through 22, the fixed MS Gothic size,
Choose font and Cancel actions, keyboard activation, persistence, and EN/ID/JP
dialogs at 200% system text. They check visible row bounds, tap targets and
untruncated labels. DC2DM must not receive these DC4 controls. Fixtures stay in
the QA application's private directory. Screenshots are saved in
`display-settings-checks/` under its external-files directory.

With `-Pandroid.injected.build.abi=arm64-v8a`, this AGP configuration writes the
QA APKs to `android/app/build/intermediates/apk/debug/app-debug.apk` and
`android/app/build/intermediates/apk/androidTest/debug/app-debug-androidTest.apk`.
Use the paths from Gradle's `apk_ide_redirect_file` listing redirects; older
files can remain in `outputs/apk/`. Quote the complete `-P...` argument when
invoking Gradle directly from PowerShell. Install these test-only APKs with
`adb install -r -t`.

Host preference checks use the same Java/Android SDK compile classpath as
`LibraryChecks` above; include `tests/android/DisplayPreferencesChecks.java`
in that compilation and run:

```powershell
java -cp build/java-check org.dc4engine.DisplayPreferencesChecks
```

These 20 checks cover defaults, per-game persistence, old libraries, malformed
values, font-size limits and migration from the old sakura preference independently
of the older archive harness. The device checks also open the in-game Visual
effect entry and verify that a change is queued for the engine thread.
See `docs/dc4-display-settings-validation.md` for native rendering, device
results, recording analysis and the tested APK. The combined ambient-effect
implementation, native save restoration and script replay evidence are recorded
in `docs/dc4-visual-effects-validation.md`.

## Fit to Screen and Stretch

With the isolated QA app and instrumentation installed, run:

```powershell
adb shell am instrument -w -e screenOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

This checks Fit/Stretch geometry on wide, portrait, 4:3, 16:9 and fractional
surface sizes, inverse touch coordinates, movie sizing, composed sprite/effect/UI
markers, native overlays and the MediaPlayer TextureView transform. On API 33+
it also renders nonuniform scaling through Off, Bilinear and FXAA on the GPU.
The real Activity SurfaceView receives synthetic pointer events; no native game
scenario is launched. Settings checks cover per-game defaults/persistence,
immediate changes, Cancel, keyboard activation, the in-game entry, and EN/ID/JP
at 200% text in a 380dp dialog.

Screenshots are saved in the QA external-files `screen-mode-checks/` directory.
Optionally place an existing 1280x720 game screenshot at
`screen-mode-inputs/title.png` there to capture both presentation modes.

Compile `tests/android/ScreenPreferencesChecks.java` alongside the same sources
as `LibraryChecks`, then run `org.dc4engine.ScreenPreferencesChecks` for twenty host
checks of Fit/Stretch and fullscreen defaults, independent game settings,
persistence and malformed values. The device suite also covers fullscreen
navigation visibility, touch mapping, focus regain and return to the library.
See `docs/screen-mode-validation.md` and `docs/fullscreen-comu-gear-validation.md`
for results and presentation scope.

## Original movie decoder probe

Build FFmpeg first, then configure the Android CMake project with
`-DDC4_BUILD_MOVIE_PROBE=ON`, the NDK toolchain, and the desired Android ABI.
Build target `movie_probe`. Compile `MovieProbe.java` with `javac --release 8`
and convert it to `classes.dex` with the SDK's `d8`.

Push `classes.dex`, `libmovie_probe.so`, `libarisa32.so`, `libc++_shared.so`,
and the five FFmpeg libraries into `/data/local/tmp/arisa32-movie-probe/`.
Use copies of existing movies in the same temporary directory; no encoding
or changes to installed game data are required.

```sh
adb shell 'CLASSPATH=/data/local/tmp/arisa32-movie-probe/classes.dex LD_LIBRARY_PATH=/data/local/tmp/arisa32-movie-probe app_process /data/local/tmp MovieProbe /data/local/tmp/arisa32-movie-probe --duration=120000 --audio /data/local/tmp/arisa32-movie-probe/original.mpg'
```

The default probe lasts three seconds per movie with no audio device. `--audio`
opens the device mixer and tests the audio-driven clock; `--duration=` sets
the wall-clock limit for subsequent files. The result reports decoded frames,
audio peak, changing pixels, pause behavior, direct bitmap copies, padded-row
preservation, completion, errors, and CPU time for update plus frame copy.
This timing excludes Canvas/GPU presentation and is not a screen frame-rate
measurement. Check `finished=1` for a complete-playback run and `error=` for
decoder errors; hardware selection appears in the FFmpeg/Android logs.

### Audio timing and silent endings

The portable `movie_audio_queue_test` CTest target checks exact five-second
silence, bounded feeding, sample rounding, overlaps, and recovery trimming.
It does not require FFmpeg or a device.

Generate independent test media with a desktop FFmpeg installation:

```powershell
.\tests\android\make_movie_regressions.ps1
```

The script uses only generated patterns and tones. It validates packet timestamps
with `ffprobe` and writes the exact commands and metadata to
`work/movie-regression/manifest.json`. Existing fixtures are revalidated; use
`-Regenerate` to replace them. `-Ffmpeg` and `-Ffprobe` accept explicit tool paths.

Push the three generated files into the probe directory and run them with
`--audio --duration=15000`. Expect 210, 240 and 90 video frames respectively for
`delayed-audio.mkv`, `silent-tail.mpg` and `synchronized-control.mpg`, each with
`finished=1` and no error. The silent-tail fixture contains over 2 MiB of
compressed video after its one-second soundtrack ends; this exercises demux
backpressure with an active audio clock.

The probe's audio peak and frame counts describe decoded/prebuffered sound, not
the instant a speaker emits it. Exact silent-prefix timing is asserted by the
portable PCM queue test; device runs establish complete decoder playback.

The generator also creates `slow-poll-h264.mp4`: 360 frames of 1080p60 H.264.
Run it without `--audio`, with `--duration=15000` and the environment variable
`ARISA32_MOVIE_PROBE_INTERVAL_MS=200`. This simulates a slow presentation loop.
Require all 360 frames, `finished=1`, and an empty error. A completion flag alone
does not detect the former duration-based cutoff with undecoded frames pending.

For a pause near the final decoder drain, run the same fixture with `--audio`
and `--duration=20000`, and set `ARISA32_MOVIE_PROBE_PAUSE_AT_MS=5900` plus
`ARISA32_MOVIE_PROBE_PAUSE_MS=6000`. Require `long_pause_ms=6000`, `pause_ok=1`,
all 360 frames, and an empty error. Time spent paused must not expire the
five-second decoder drain timeout.

## GLES compatibility without native game libraries

The standalone project copies the exact production FrameFilter,
GlesFrameRenderer, SurfaceFrameRenderer and ScreenViewport sources at build time.
It needs no ARM game libraries, so it can run on API 21+ x86 emulators as well
as physical phones. Set ANDROID_HOME and use the repository Gradle wrapper:

```powershell
android/gradlew.bat -p tests/android/gles-compat assembleDebug assembleDebugAndroidTest lintDebug
adb install -r -t tests/android/gles-compat/build/outputs/apk/debug/gles-compat-debug.apk
adb install -r -t tests/android/gles-compat/build/outputs/apk/androidTest/debug/gles-compat-debug-androidTest.apk
adb shell am instrument -w org.dc4engine.glesqa.test/org.dc4engine.GlesCompatChecks
```

GlesFrameChecks verifies actual GPU pixel changes, updates to a reused bitmap,
Off/Bilinear/FXAA switching, shader failure, fit/stretch, movie aspect and texture
resizing, premultiplied overlays, window presentation, context recovery, surface
replacement and Canvas fallback across a renderer restart. GPU readback is
confined to this test. Captures are in external-files/gles-frame-checks.
Use an explicit adb serial when several devices are connected. Pull evidence
before uninstalling org.dc4engine.glesqa.test and org.dc4engine.glesqa.
See docs/fxaa-legacy-validation.md for observed coverage and limitations.

## CG preview back button

Build with `update-qa.init.gradle` and install the isolated QA app and test APKs.
The fixture generator uses private QA storage and requires no installed games.

```powershell
adb shell am instrument -w -e galleryOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

`GalleryPreviewChecks` exercises DC4 and Plus Harmony menu formats in English,
Indonesian and Japanese. It checks that the button appears only after opening a
CG, follows navigation auto-hide and reveal, and returns to the same gallery through touch,
Android Back and keyboard Enter. It also checks variant taps, duplicate close
requests, render pause/resume, safe bounds, 48dp targets, 2x text and a 380dp
container. Screenshots are in external-files/gallery-preview-checks.

The same suite also checks complete 1920x1080 CG corners, an intermediate
left-to-right mask transition, advancement beyond the selected thumbnail,
skipping locked CGs, portrait containment, and route wrapping. Native
`system_menu_test <AdvData> --gallery` adds deterministic elapsed-time checks
and compares rendered pixels with original DC4/Plus Harmony image assets.
See `docs/cg-preview-render-validation.md` for the rendering results.

Use `sidebarOnly` and `navigationOnly` for the existing navigation regressions.
For installed desktop assets, run `system_menu_test <AdvData> --menus` to check
native menus, including the gallery preview state, without the replay catalogue
test that assumes the DC4 All Age scene count. See
`docs/cg-preview-back-validation.md` for results and test limits.

## Incomplete DC4 base folders

Build the QA application and instrumentation with `update-qa.init.gradle`, then:

```powershell
adb shell am instrument -w -e gameDataOnly true org.dc4engine.updateqa.test/org.dc4engine.UpdateDialogChecks
```

The 64 checks exercise registration and launching of complete and partial
folders with identical MES fingerprints, overlay acceptance, catalogue headers,
EN/ID/JP error dialogs, and native title startup for DC4 and DC4PH. Fixtures
use a separate temporary library. As with the other instrumentation modes,
the runner changes QA launcher preferences; back up and restore those if the
QA installation has user settings. See `docs/dc4ph-startup-validation.md`.

## DC2DM save and progress probe

Use an arm64 device, a built APK, and a local copy of the PC SaveData folder:

```powershell
python tests/android/run_dc2dm_save_probe.py --apk android/app/build/outputs/apk/debug/app-debug.apk --saves path/to/copied/SaveData --serial DEVICE_SERIAL --data "/sdcard/Gamehub/Da Capo 2 Dearest Marriage/AdvData" --exe "/sdcard/Gamehub/Da Capo 2 Dearest Marriage/DC2DM.EXE" --font path/to/msgothic.ttc
```

The copied folder must contain `system.dat`, `DC2DM01.dat`, `DC2DM02.dat`,
`DC2DM03.dat`, `DC2DM04.dat`, and `DC2DM06.dat`. The inspected executable must
contain the 279-entry scenario-title table. JDK and Android SDK paths use
`JAVA_HOME` and `ANDROID_HOME` when set, otherwise the local Windows defaults.

The default run has 12 cases: three title stages, a separate-process restart
for each stage, five native/companion save round trips, and Save/Load rows.
Restart cases remove their native `system.dat` before starting again. Slot
cases wait for transitions, advance empty checkpoints, compare restored
dialogue/speaker/artwork, and continue the story. The row case checks the
Save/Load confirmation path for quick, auto, latest, and regular slots.

Stage cases also tap the real Start button. Before Graduation completion,
they exercise the native confirmation's Cancel, No, and Yes paths. After
Graduation or Married Life completion, Start must directly open the two- or
three-story selector. The probe checks responding rows and the native Return
button. An idle selector waits for input, so its script-wait status is not an
animation-completion signal.

The optional `--cases menus` imports copied progress into the fixture and checks
Extras, CG, Scenario, Music, Config, all four Otome voice choices, full CG
rendering at 1920x1080, next/back navigation, and the return-to-title particle
transition with pause/resume. It writes menu, CG, and transition captures.
Use `--cases menus stages DC2DM01.dat DC2DM02.dat DC2DM03.dat DC2DM04.dat DC2DM06.dat rows`
for all 13 cases. See [the menu and CG report](../../docs/dc2dm-menu-audio-transition-validation.md).

Use `--cases stages`, `--cases rows`, or a list such as
`--cases DC2DM02.dat DC2DM06.dat` to repeat selected cases. `stages` always runs
all three first-run/restart pairs. A failed case stops the run and returns a
nonzero exit code; completed output is saved after every case.

For the recorded birthday choice, run `scene_test` with `DC2DM_DATA` and
`DC2DM_CHOICE_CAPTURES` to export its native `choice.dat`, then add
`--choice-save path/to/choice.dat --cases choices` to the probe command.
This optional case checks both `MAR_0129_E1` branches through the JNI touch
handler, including normal, hover and pressed row art, cancel, release over
another row, and outside taps. It checks that Yoshiyuki and narration cannot
replay stale audio, that the next Anzu line can replay, and that restoring
an unvoiced save after Anzu keeps replay disabled. Captures show both choices
and their first unvoiced/voiced dialogue. It uses the same isolated saves as
the other cases and does not install or launch the player's Activity.

For the recorded classroom transition, run `dc2dm_transition_test` with
`DC2DM_DATA` and `DC2DM_TRANSITION_CAPTURES` to export `transition.dat`, then
add `--transition-save path/to/transition.dat --cases transitions`. This
checks the two mask phases, white boundary, direction and soft edge, pause,
dialogue blocking, transient-save rejection, forced Title, and subsequent
Load. Transition captures and result logs use the same isolated fixture.

The probe runs the packaged JNI library in `app_process`; it does not install
the APK or launch its Activity. It links only game asset directories into a
unique `/data/local/tmp/arisa32-dc2dm-save-*` fixture so profile discovery cannot
mix the player's progress with the staged flags. All saves are written there.
Logs, APK/JNI hashes, and captures are copied to `build/dc2dm-save-validation/`.
See [the validation report](../../docs/dc2dm-save-progress-validation.md) for
results and the limits of native-PC compatibility.

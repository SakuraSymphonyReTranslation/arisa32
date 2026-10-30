# Android library and save-archive checks

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
download/release intents. It tests the actual v0.6.2 Indonesian and English notes
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

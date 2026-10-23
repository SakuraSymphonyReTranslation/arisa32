# Android library and save-archive checks

Profile checks need a Java 11+ JDK and no Android runtime:

```powershell
javac -encoding UTF-8 -d build/java-check `
  android/app/src/main/java/org/dc4engine/GameLibrary.java `
  (Get-ChildItem android/app/src/main/java/org/dc4engine/profile/*.java).FullName `
  tests/android/ProfileChecks.java
java -cp build/java-check org.dc4engine.ProfileChecks
```

They verify MES-header detection, profile precedence, neighboring dialect
isolation, library titles, and the separate DC2PC save namespace.

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

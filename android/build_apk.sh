#!/usr/bin/env bash
# Build the same Android project used by Android Studio, including FFmpeg.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/android"
case "$(uname -s)" in
  MINGW*|MSYS*) ./gradlew.bat assembleDebug --console=plain "$@" ;;
  *) bash ./gradlew assembleDebug --console=plain "$@" ;;
esac

APK="$ROOT/android/app/build/outputs/apk/debug/app-debug.apk"
test -f "$APK"
cp "$APK" "$ROOT/android/Arisa32-debug.apk"
cp "$APK" "$ROOT/android/dc4-debug.apk"
printf 'APK: %s\n' "$ROOT/android/Arisa32-debug.apk"

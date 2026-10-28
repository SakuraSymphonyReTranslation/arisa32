# Android FFmpeg validation — 27 October 2026

Android uses FFmpeg 7.1.3 with MediaCodec where available and software decoding
otherwise. Original game and patch files are read directly, without transcoding.
The desktop player retains its existing MPEG-1 backend.

## Build and package checks

- `gradlew.bat assembleDebug assembleRelease --console=plain` succeeded for both
  `arm64-v8a` and `armeabi-v7a` after the audio fixes.
- Both APKs contain seven shared libraries per ABI, the bundled font, and the new
  audio code. No VLC entries or Videolan references remain in the APKs.
- Both APKs pass `apksigner verify` and `zipalign -c -P 16 4`. All ARM64 ELF load
  segments meet the 16 KiB alignment requirement. The NDK's 32-bit libc++ runtime
  retains its expected 4 KiB segments.
- The release APK was installed successfully on the connected I2219 / SM6450
  device without clearing application data.
- All 16 desktop CTest targets passed. `save_host_test` initially encountered a
  sandbox denial in the system temporary directory, then passed with `TEMP`/`TMP`
  set to `build/test-tmp`. The final `movie_audio_queue_test` rerun also passed.
- MSVC syntax checking and the ARM64 NDK decoder-probe build succeeded.
- `git diff --check` passed.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `android/Arisa32-debug.apk` | 52,194,142 | `d1b366b26f6703dba5903ba90dcc0de8df796fc5e8dc2ef68073e3ab5b3688de` |
| `android/app/build/outputs/apk/release/app-release.apk` | 13,117,082 | `ba25c1814c1a96d48d13ddfcd33c911e00d9b70bc3b00eeecb045df0f6ff8c30` |

`android/dc4-debug.apk` is the same debug build. Package details are recorded in
`work/movie-apk-verification.json`. The release build currently uses the project's
debug signing configuration.

## Full original-stream playback

The rebuilt decoder completed every original sample with an active audio device,
successful frame/stride copies and no decoder error:

| Stream | Frames | Final movie time | Mean update + frame copy |
| --- | ---: | ---: | ---: |
| MPEG-1, 800 × 600 | 3,370 | 112.367 s | 6.751 ms |
| WMV, 1280 × 720 | 2,998 | 100.107 s | 16.407 ms |
| HEVC patch, 1280 × 720 | 3,279 | 109.320 s | 8.234 ms |

Results: `work/movie-probe-completion-current.txt`. HEVC used
`c2.qti.hevc.decoder`; its sample retains the historical filename
`existing-h264.mpg`. MPEG-1 and WMV used software decoding. The WMV movie starts
and ends on identical pixels; a prior short probe confirmed changing pixels.

These timings exclude Canvas/GPU presentation. They are not screen frame-rate
measurements or performance guarantees for other chipsets.

## Audio regression checks

The timestamped PCM queue preserves delayed soundtracks while feeding a bounded
lead into the mixer. Demuxing retains the current compressed packet when the
video queue fills. With no queued sound, the clock can advance through a silent
video ending and release that space. Recovery resets/rebases the same timeline.
Resampler delay and coarse container timestamps are handled without introducing
small gaps in continuous audio; lead comparisons use sample-rounded positions.

`movie_audio_queue_test` verifies the exact five-second silent prefix, bounded
feeding and failed-sink retry, overlaps, stale-sample trimming, the timeline
reset/trimming used during recovery, and fractional-sample lead completion.
It does not exercise a live MediaCodec-to-software recovery.

`tests/android/make_movie_regressions.ps1` creates only synthetic patterns and
tones. It reads no game assets. `work/movie-regression/manifest.json` records
exact commands and `ffprobe` packet metadata. On the device:

| Sample | Expected content | Result with rebuilt decoder |
| --- | --- | --- |
| `delayed-audio.mkv` | 7 s video; audio packets start at exactly 5 s | 210 frames, finished at 7.006 s, no error |
| `silent-tail.mpg` | 8 s video; 1.019 s audio; 8,802,083 compressed video bytes after audio ends | 240 frames, finished at 8.020 s, no error |
| `synchronized-control.mpg` | 3 s synchronized video/audio | 90 frames, finished at 3.022 s, no error |

The old decoder, extracted from the previous debug APK, reproduced the silent-tail
failure at 0.740 s after 22 frames:
`movie stream interleave exceeds the compressed-buffer limit`.
The rebuilt decoder played the same file to completion. Results are in
`work/movie-probe-regression-before.txt` and
`work/movie-probe-regression-current.txt`.

Audio peak and frame counts in the device probe describe decoded/prebuffered
sound. The exact silent-prefix assertion comes from the PCM queue test, not from
the device probe's peak metric.

## App integration and limits

Static JNI/Java review covers bitmap reuse and forced copies after recreation,
render-thread ownership, config pause/resume, and return to the menu or scene.

The installed release app rendered actual movie frames. Its PID-scoped log
(`build/device-qa/ffmpeg-delivery-app-log.txt`, PID 15774) records native FFmpeg
playback of Plus Harmony OP1/OP2 (MPEG-4/AAC), OP3 (MPEG-2/AAC), and translated
DC4 OP2/OP3 (MPEG-1/MP2), all using software decoding. No fatal exception or
fatal signal appears in that captured app log.

A movie frame was captured before sending Home and reopening the activity.
The immediate post-launch screenshot caught a launcher/recents transition,
so it does not establish uninterrupted pause/resume or audio synchronization.
A later screenshot (`build/device-qa/ffmpeg-delivery-current.png`) shows the
movie-selection menu, with the same app PID and a resumed Arisa32Activity.
Device navigation changed independently during these checks; movie playback
and a later return to the menu were observed, but the exact completion/skip
route and controlled pause/resume remain unverified. The full-decoder probe's
`pause_ok` check runs after playback and is not evidence for in-flight resume.
All app screenshots are retained under `build/device-qa/`.

Runtime measurements cover this ARM64 device. The 32-bit ABI is built and
packaged, but was not run on a 32-bit device. Performance on Helio P35/G81 or
Snapdragon 660 still needs those actual devices.

The initial SDK/ADB permission blockage was resolved by the user before the
successful builds and device checks above.

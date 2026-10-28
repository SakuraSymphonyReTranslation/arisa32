# Android FFmpeg decoder build

FFmpeg 7.1.3 is built from the checksum-verified release archive by
`android/build_ffmpeg.sh`. On Windows run `android/build_ffmpeg_windows.bat`
to supply Visual C++ headers for host table generators. The build requires
Git Bash, Visual Studio C++ build tools, and Android NDK 27.3.13750724.
Linux/WSL builds use the Linux NDK and the system C compiler.

Generated headers and shared libraries are placed in `android/<ABI>/`.
Both `arm64-v8a` and `armeabi-v7a` are required before a regular APK build.
These are generated build outputs, not the old prebuilt FFmpeg 3.3 binaries.

Only local demuxers, selected video/audio decoders, scaling, and audio
resampling are included. Encoders, muxers, network protocols, filters, GPL
and nonfree components are disabled. The enabled components use LGPL 2.1
or later. Shared linking retains replaceable FFmpeg libraries. The build
script includes the upstream LGPL license texts; retain them and provide
the matching FFmpeg sources and build script when distributing binaries.

MediaCodec decodes supported original streams. MPEG-1 and WMV/WMA use the
optimized software decoder when the device has no matching hardware codec.
NEON/assembly optimization remains enabled for both Android ABIs. No game
movie is converted or written by this build or the player.

H.264 and HEVC hardware decoders are included for original patch files, even
when their extension is `.mpg`. The Windows build patches the upstream link
recipe to use Clang response files, avoiding the command-length limit with
the selected decoder object files. This patch is applied by the build script.

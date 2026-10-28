#!/usr/bin/env bash
# Decoder-only FFmpeg for Android. Run in Git Bash on Windows or bash on Linux/WSL.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION=7.1.3
SHA256=f0bf043299db9e3caacb435a712fc541fbb07df613c4b893e8b77e67baf3adbe
WORK="$ROOT/work/ffmpeg"
DEST="$ROOT/third_party/ffmpeg/android"
NDK="${ANDROID_NDK_HOME:-${ANDROID_SDK_ROOT:-${LOCALAPPDATA:-}/Android/Sdk}/ndk/27.3.13750724}"
case "$(uname -s)" in
  MINGW*|MSYS*)
    NDK="$(cygpath -u "$NDK")"
    HOST=windows-x86_64
    export PATH="$NDK/prebuilt/$HOST/bin:$NDK/toolchains/llvm/prebuilt/$HOST/bin:$PATH"
    export MSYS2_ARG_CONV_EXCL='--sysroot='
    SYSROOT="$(cygpath -m "$NDK/toolchains/llvm/prebuilt/$HOST/sysroot")"
    HOST_CC='clang --target=x86_64-pc-windows-msvc'
    ;;
  Linux*) HOST=linux-x86_64; SYSROOT="$NDK/toolchains/llvm/prebuilt/$HOST/sysroot"; HOST_CC=cc
    export PATH="$NDK/toolchains/llvm/prebuilt/$HOST/bin:$PATH" ;;
  *) echo 'Use Git Bash on Windows or bash on Linux/WSL.' >&2; exit 1 ;;
esac
test -d "$SYSROOT" || { echo "NDK missing: $NDK" >&2; exit 1; }
mkdir -p "$WORK" "$DEST"
ARCHIVE="$WORK/ffmpeg-$VERSION.tar.xz"
if [ ! -f "$ARCHIVE" ]; then
  curl --fail --location --retry 3 "https://ffmpeg.org/releases/ffmpeg-$VERSION.tar.xz" -o "$ARCHIVE"
fi
echo "$SHA256  $ARCHIVE" | sha256sum --check
if [ ! -d "$WORK/ffmpeg-$VERSION" ]; then tar -xf "$ARCHIVE" -C "$WORK"; fi
if [ "$HOST" = windows-x86_64 ]; then
  # HEVC pushes the shared-library link command past cmd.exe's 8191-byte limit.
  # GNU make writes a Clang response file without passing the object list to a shell.
  sed -i 's|^\(\t\)\$\$(LD) \(.*\)$|\1$$(file >$$@.rsp,\2)$$(LD) @$$@.rsp|' \
    "$WORK/ffmpeg-$VERSION/ffbuild/library.mak"
fi
for ABI in "${@:-arm64-v8a armeabi-v7a}"; do
  # The default word list is split intentionally; explicit ABI arguments also work.
  for ABI_NAME in $ABI; do
    case "$ABI_NAME" in
      arm64-v8a) ARCH=aarch64; CPU=armv8-a; TRIPLE=aarch64-linux-android21 ;;
      armeabi-v7a) ARCH=arm; CPU=armv7-a; TRIPLE=armv7a-linux-androideabi21 ;;
      *) echo "Unsupported ABI: $ABI_NAME" >&2; exit 1 ;;
    esac
    BUILD="$WORK/build-$ABI_NAME"
    mkdir -p "$BUILD" "$DEST/$ABI_NAME"
    cd "$BUILD"
    PREFIX="$DEST/$ABI_NAME"
    # MinGW GNU make must receive Windows paths in generated install rules.
    if [ "$HOST" = windows-x86_64 ]; then PREFIX="$(cygpath -m "$PREFIX")"; fi
    bash "$WORK/ffmpeg-$VERSION/configure" \
      --prefix="$PREFIX" --target-os=android --arch="$ARCH" --cpu="$CPU" \
      --enable-cross-compile --cc=clang --cxx=clang++ --ld=clang --host-cc="$HOST_CC" \
      --ar=llvm-ar --ranlib=llvm-ranlib --nm=llvm-nm --strip=llvm-strip \
      --extra-cflags="--target=$TRIPLE --sysroot=$SYSROOT -fPIC" --optflags=-O2 \
      --extra-ldflags="--target=$TRIPLE --sysroot=$SYSROOT -Wl,-z,max-page-size=16384" \
      --enable-shared --disable-static --enable-pic --enable-small \
      --disable-everything --disable-autodetect --disable-programs --disable-doc \
      --disable-debug --disable-network --disable-avdevice --disable-avfilter \
      --disable-postproc --disable-symver --enable-jni --enable-mediacodec \
      --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample \
      --enable-protocol=file \
      --enable-demuxer=asf,mpegps,mpegts,mpegvideo,mov,avi,matroska,ogg \
      --enable-decoder=mpeg1video,mpeg2video,mpeg4,h264,hevc,wmv1,wmv2,wmv3,vc1,theora,vp8,vp9 \
      --enable-decoder=h264_mediacodec,hevc_mediacodec,mpeg2_mediacodec,mpeg4_mediacodec,vp8_mediacodec,vp9_mediacodec \
      --enable-decoder=mp2,mp3,mp3float,aac,wmav1,wmav2,wmapro,wmavoice,vorbis,opus,flac,pcm_s16le,pcm_s16be,pcm_u8 \
      --enable-parser=mpegvideo,mpeg4video,h264,hevc,vc1,mpegaudio,aac,vorbis,opus,vp8,vp9 \
      --enable-bsf=h264_mp4toannexb,hevc_mp4toannexb,mpeg4_unpack_bframes
    if [ "$HOST" = windows-x86_64 ]; then
      # NDK's native Windows GNU make does not understand MSYS /e/... paths.
      WIN_SOURCE="$(cygpath -m "$WORK/ffmpeg-$VERSION")"
      sed -i "s|$WORK/ffmpeg-$VERSION|$WIN_SOURCE|g" Makefile ffbuild/config.mak
    fi
    make -j"${FFMPEG_JOBS:-4}" > build.log 2>&1 || { tail -60 build.log; exit 1; }
    make install-libs > install.log 2>&1 || { tail -60 install.log; exit 1; }
    # Native Windows make hits cmd.exe's command-length limit when upstream
    # installs the complete avutil header list in one command. Copy individually.
    for COMPONENT in avcodec avformat avutil swscale swresample; do
      mkdir -p "$DEST/$ABI_NAME/include/lib$COMPONENT"
      for HEADER in "$WORK/ffmpeg-$VERSION/lib$COMPONENT/"*.h; do
        cp "$HEADER" "$DEST/$ABI_NAME/include/lib$COMPONENT/"
      done
    done
    cp libavutil/avconfig.h libavutil/ffversion.h "$DEST/$ABI_NAME/include/libavutil/"
    printf '%s\n' "FFmpeg $VERSION ($ABI_NAME) built in $BUILD"
  done
done
cp "$WORK/ffmpeg-$VERSION/COPYING.LGPLv2.1" "$DEST/../COPYING.LGPLv2.1"
cp "$WORK/ffmpeg-$VERSION/COPYING.LGPLv3" "$DEST/../COPYING.LGPLv3"

cmake_minimum_required(VERSION 3.22)
project(dc4android CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# dc4core sources live at the repository root.
get_filename_component(DC4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(DC4_CORE "${DC4_ROOT}/core")

set(DC4_CORE_SRC
    ${DC4_CORE}/src/crx.cpp
    ${DC4_CORE}/src/crm.cpp
    ${DC4_CORE}/src/mesfile.cpp
    ${DC4_CORE}/src/messtream.cpp
    ${DC4_CORE}/src/mesopcode.cpp
    ${DC4_CORE}/src/names.cpp
    ${DC4_CORE}/src/mesvm.cpp
    ${DC4_CORE}/src/voice.cpp
    ${DC4_CORE}/src/xpcm.cpp
    ${DC4_CORE}/src/bgm.cpp
    ${DC4_CORE}/src/audio.cpp
    ${DC4_CORE}/src/scene_audio.cpp
    ${DC4_CORE}/src/scene.cpp
    ${DC4_CORE}/src/scene_state.cpp
    ${DC4_CORE}/src/log_screen.cpp
    ${DC4_CORE}/src/save_screen.cpp
    ${DC4_CORE}/src/resource.cpp
    ${DC4_CORE}/src/system_menu.cpp
    ${DC4_CORE}/src/movie_ffmpeg.cpp
    ${DC4_CORE}/src/movie_resource.cpp
    ${DC4_CORE}/src/confirm_dialog.cpp
    # miniz (zlib inflate for CRX) — vendored C sources
    ${DC4_ROOT}/third_party/miniz/miniz.c
    ${DC4_ROOT}/third_party/miniz/miniz_tinfl.c
    ${DC4_ROOT}/third_party/miniz/miniz_tdef.c
)
set_source_files_properties(${DC4_ROOT}/third_party/miniz/miniz.c
    ${DC4_ROOT}/third_party/miniz/miniz_tinfl.c
    ${DC4_ROOT}/third_party/miniz/miniz_tdef.c PROPERTIES LANGUAGE C)

add_library(arisa32 SHARED
    ${DC4_CORE_SRC}
    arisa32_jni.cpp
)

target_include_directories(arisa32 PRIVATE
    ${DC4_CORE}/include
    ${DC4_ROOT}/third_party/miniz
    ${DC4_ROOT}/third_party/stb
    ${DC4_ROOT}/third_party/miniaudio
    ${DC4_ROOT}/third_party/pl_mpeg
)

# miniaudio: OpenSL|ES on every supported API (AAudio is disabled).
target_compile_definitions(arisa32 PRIVATE
    MA_NO_RUNTIME_LINKING
    MA_NO_AAUDIO
)

find_library(log-lib log)
find_library(android-lib android)
find_library(openmaxndk OpenMAXAL)
find_library(opensles-lib OpenSLES)
find_library(jnigraphics-lib jnigraphics)
target_link_libraries(arisa32 PRIVATE
    ${log-lib}
    ${android-lib}
    ${openmaxndk}
    ${opensles-lib}
    ${jnigraphics-lib}
)

# Support 16 KB page sizes for Android 15 and Android 16 (Baklava)
target_link_options(arisa32 PRIVATE "-Wl,-z,max-page-size=16384")

# Built once with android/build_ffmpeg.sh; Android Studio then builds normally.
set(FFMPEG_ROOT "${DC4_ROOT}/third_party/ffmpeg/android/${ANDROID_ABI}")
foreach(component avformat avcodec swscale swresample avutil)
    if(NOT EXISTS "${FFMPEG_ROOT}/lib/lib${component}.so")
        message(FATAL_ERROR "Missing FFmpeg for ${ANDROID_ABI}. Run bash android/build_ffmpeg.sh first.")
    endif()
    add_library(ffmpeg_${component} SHARED IMPORTED)
    set_target_properties(ffmpeg_${component} PROPERTIES
        IMPORTED_LOCATION "${FFMPEG_ROOT}/lib/lib${component}.so"
        INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include")
    target_link_libraries(arisa32 PRIVATE ffmpeg_${component})
endforeach()

# Optional isolated device probe, excluded from APK builds.
option(DC4_BUILD_MOVIE_PROBE "Build the app_process movie decoder probe" OFF)
if(DC4_BUILD_MOVIE_PROBE)
    add_library(movie_probe SHARED "${DC4_ROOT}/tests/android/movie_probe.cpp")
    target_include_directories(movie_probe PRIVATE "${DC4_CORE}/include")
    target_link_libraries(movie_probe PRIVATE arisa32 ffmpeg_avcodec)
    target_link_options(movie_probe PRIVATE "-Wl,-z,max-page-size=16384")
endif()


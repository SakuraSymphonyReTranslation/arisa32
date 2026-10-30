cmake_minimum_required(VERSION 3.20)
project(arisa32 CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_library(arisa32_core STATIC
    core/src/messtream.cpp
    core/src/mesopcode.cpp
    core/src/names.cpp
    core/src/mesfile.cpp
    core/src/mesvm.cpp
    core/src/voice.cpp
    core/src/xpcm.cpp
    core/src/bgm.cpp
    core/src/scene.cpp
    core/src/scene_state.cpp
    core/src/scene_dc2dm_ending.cpp
    core/src/log_screen.cpp
    core/src/save_screen.cpp
    core/src/resource.cpp
    core/src/system_menu.cpp
    core/src/movie.cpp
    core/src/movie_resource.cpp
    core/src/confirm_dialog.cpp
)
add_library(dc4core ALIAS arisa32_core)
target_include_directories(arisa32_core PUBLIC core/include)
target_include_directories(arisa32_core PRIVATE third_party/pl_mpeg third_party/stb)

add_executable(mesdis tools/cpp/mesdis.cpp)
target_link_libraries(mesdis PRIVATE arisa32_core)

add_executable(voicetool tools/cpp/voicetool.cpp)
target_link_libraries(voicetool PRIVATE arisa32_core)

target_sources(arisa32_core PRIVATE core/src/crx.cpp)
target_include_directories(arisa32_core PRIVATE third_party/miniz)

add_executable(crx_tool tools/cpp/crx_tool.cpp)
target_link_libraries(crx_tool PRIVATE arisa32_core)
target_include_directories(crx_tool PRIVATE third_party/stb)

target_sources(arisa32_core PRIVATE core/src/crm.cpp)
add_executable(crm_tool tools/cpp/crm_tool.cpp)
target_link_libraries(crm_tool PRIVATE arisa32_core)
target_include_directories(crm_tool PRIVATE third_party/stb)

target_sources(arisa32_core PRIVATE
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_tdef.c
)
set_source_files_properties(third_party/miniz/miniz.c third_party/miniz/miniz_tinfl.c third_party/miniz/miniz_tdef.c PROPERTIES LANGUAGE C)

# --- desktop SDL2 app ---
add_executable(arisa32_desktop sdl_desktop/main.cpp sdl_desktop/ui_script.cpp)
target_link_libraries(arisa32_desktop PRIVATE arisa32_core)
add_executable(dc4_desktop ALIAS arisa32_desktop)
if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The bundled SDL2 library requires an x64 build (run build.bat).")
    endif()
    set(DC4_SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sdl2/SDL2-2.32.10"
        CACHE PATH "SDL2 development package containing include/ and lib/x64/")
    target_include_directories(arisa32_desktop PRIVATE "${DC4_SDL2_ROOT}/include")
    target_link_libraries(arisa32_desktop PRIVATE "${DC4_SDL2_ROOT}/lib/x64/SDL2.lib")
    add_custom_command(TARGET arisa32_desktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DC4_SDL2_ROOT}/lib/x64/SDL2.dll" $<TARGET_FILE_DIR:arisa32_desktop>
        VERBATIM)
else()
    find_package(SDL2 2.0.18 REQUIRED)
    find_package(Threads REQUIRED)
    target_link_libraries(arisa32_desktop PRIVATE SDL2::SDL2)
    target_link_libraries(arisa32_core PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)
endif()
target_compile_definitions(arisa32_desktop PRIVATE SDL_MAIN_HANDLED)
target_include_directories(arisa32_desktop PRIVATE third_party/stb)
target_sources(arisa32_core PRIVATE core/src/audio.cpp core/src/scene_audio.cpp)
target_include_directories(arisa32_core PRIVATE third_party/miniaudio third_party/stb)

include(CTest)
if(BUILD_TESTING)
    add_executable(dc2pc_menu_transition_test tests/dc2pc_menu_transition_test.cpp)
    target_include_directories(dc2pc_menu_transition_test PRIVATE core/include)
    add_test(NAME dc2pc_menu_transition_test COMMAND dc2pc_menu_transition_test)
    add_executable(crm_test tests/crm_test.cpp)
    target_link_libraries(crm_test PRIVATE dc4core)
    add_test(NAME crm_test COMMAND crm_test)
    add_executable(dc2pc_flow_test tests/dc2pc_flow_test.cpp)
    target_link_libraries(dc2pc_flow_test PRIVATE dc4core)
    add_test(NAME dc2pc_flow_test COMMAND dc2pc_flow_test)
    add_executable(text_render_test tests/text_render_test.cpp)
    target_link_libraries(text_render_test PRIVATE dc4core)
    add_test(NAME text_render_test COMMAND text_render_test
        "${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/assets/fonts/MSGOTHIC.TTC")
    add_executable(mesopcode_test tests/mesopcode_test.cpp)
    target_link_libraries(mesopcode_test PRIVATE dc4core)
    add_test(NAME mesopcode_test COMMAND mesopcode_test)
    add_executable(dc2pc_menu_test tests/dc2pc_menu_test.cpp)
    target_link_libraries(dc2pc_menu_test PRIVATE dc4core)
    add_test(NAME dc2pc_menu_test COMMAND dc2pc_menu_test)
    add_executable(dc2dm_menu_test tests/dc2dm_menu_test.cpp)
    target_link_libraries(dc2dm_menu_test PRIVATE arisa32_core)
    add_test(NAME dc2dm_menu_test COMMAND dc2dm_menu_test)
    add_executable(confirm_dialog_test tests/confirm_dialog_test.cpp)
    target_link_libraries(confirm_dialog_test PRIVATE dc4core)
    add_test(NAME confirm_dialog_test COMMAND confirm_dialog_test)
    add_executable(log_screen_test tests/log_screen_test.cpp)
    target_link_libraries(log_screen_test PRIVATE dc4core)
    add_test(NAME log_screen_test COMMAND log_screen_test)
    add_executable(resource_test tests/resource_test.cpp)
    target_link_libraries(resource_test PRIVATE dc4core)
    add_test(NAME resource_test COMMAND resource_test)
    add_executable(system_menu_test tests/system_menu_test.cpp)
    target_link_libraries(system_menu_test PRIVATE dc4core)
    add_test(NAME system_menu_test COMMAND system_menu_test)
    add_executable(menu_music_test tests/menu_music_test.cpp)
    target_link_libraries(menu_music_test PRIVATE dc4core)
    add_test(NAME menu_music_test COMMAND menu_music_test)
    add_executable(media_test tests/media_test.cpp)
    target_link_libraries(media_test PRIVATE dc4core)
    add_test(NAME media_test COMMAND media_test)
    add_executable(movie_audio_queue_test tests/movie_audio_queue_test.cpp)
    target_include_directories(movie_audio_queue_test PRIVATE core/src)
    add_test(NAME movie_audio_queue_test COMMAND movie_audio_queue_test)
    add_executable(mesvm_test tests/mesvm_test.cpp)
    target_link_libraries(mesvm_test PRIVATE dc4core)
    add_test(NAME mesvm_test COMMAND mesvm_test)
    add_executable(audio_test tests/audio_test.cpp)
    target_link_libraries(audio_test PRIVATE dc4core)
    add_test(NAME audio_test COMMAND audio_test)
    add_executable(scene_test tests/scene_test.cpp)
    target_link_libraries(scene_test PRIVATE dc4core)
    add_test(NAME scene_test COMMAND scene_test)
    add_executable(credits_test tests/credits_test.cpp)
    target_link_libraries(credits_test PRIVATE dc4core)
    add_test(NAME credits_test COMMAND credits_test)
    add_executable(dc2dm_ending_test tests/dc2dm_ending_test.cpp)
    target_link_libraries(dc2dm_ending_test PRIVATE dc4core)
    add_test(NAME dc2dm_ending_test COMMAND dc2dm_ending_test)
    add_executable(dc2dm_sprite_test tests/dc2dm_sprite_test.cpp)
    target_link_libraries(dc2dm_sprite_test PRIVATE dc4core)
    add_test(NAME dc2dm_sprite_test COMMAND dc2dm_sprite_test)
    add_executable(save_screen_test tests/save_screen_test.cpp)
    target_link_libraries(save_screen_test PRIVATE dc4core)
    add_test(NAME save_screen_test COMMAND save_screen_test)
    add_executable(save_host_test tests/save_host_test.cpp sdl_desktop/ui_script.cpp)
    target_link_libraries(save_host_test PRIVATE dc4core)
    target_compile_definitions(save_host_test PRIVATE SDL_MAIN_HANDLED)
    target_include_directories(save_host_test PRIVATE third_party/stb)
    if(WIN32)
        target_include_directories(save_host_test PRIVATE "${DC4_SDL2_ROOT}/include")
        target_link_libraries(save_host_test PRIVATE "${DC4_SDL2_ROOT}/lib/x64/SDL2.lib")
        add_custom_command(TARGET save_host_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DC4_SDL2_ROOT}/lib/x64/SDL2.dll" $<TARGET_FILE_DIR:save_host_test>
            VERBATIM)
    else()
        target_link_libraries(save_host_test PRIVATE SDL2::SDL2)
    endif()
    add_test(NAME save_host_test COMMAND save_host_test)
endif()

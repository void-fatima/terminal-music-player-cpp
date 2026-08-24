cmake_minimum_required(VERSION 3.16)
project(terminal_music_player VERSION 1.0.0 LANGUAGES CXX)

include(CTest)
include(FetchContent)
find_package(Threads REQUIRED)

# FTXUI 7.0.1 is pinned for reproducible, cross-platform raw keyboard input,
# resize-aware layout, Unicode cell measurement, and NO_COLOR support.
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_MODULES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v7.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

function(music_player_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(music-player-core STATIC
    src/AtomicFile.cpp
    src/ConfigManager.cpp
    src/DataLoader.cpp
    src/CliOptions.cpp
    src/InputMapping.cpp
    src/MusicLibrary.cpp
    src/PathUtils.cpp
    src/Playlist.cpp
    src/PlaylistManager.cpp
    src/Song.cpp
    src/StableId.cpp
    src/Utf8.cpp
)
target_include_directories(music-player-core PUBLIC src)
target_compile_features(music-player-core PUBLIC cxx_std_17)
music_player_warnings(music-player-core)

add_library(music-player-audio STATIC
    src/MiniaudioBackend.cpp
    src/Player.cpp
)
target_include_directories(music-player-audio PUBLIC src PRIVATE third_party/miniaudio)
target_compile_features(music-player-audio PUBLIC cxx_std_17)
target_link_libraries(music-player-audio PUBLIC music-player-core PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
    target_link_libraries(music-player-audio PRIVATE ole32 winmm uuid avrt)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(music-player-audio PRIVATE m)
endif()
music_player_warnings(music-player-audio)

add_library(music-player-application STATIC
    src/Application.cpp
    src/SessionController.cpp
    src/StreamUi.cpp
    src/TerminalUi.cpp
)
target_include_directories(music-player-application PUBLIC src)
target_compile_features(music-player-application PUBLIC cxx_std_17)
target_link_libraries(music-player-application PUBLIC music-player-core music-player-audio ftxui::ftxui)
music_player_warnings(music-player-application)

add_executable(terminal-music-player src/main.cpp)
target_compile_features(terminal-music-player PRIVATE cxx_std_17)
target_link_libraries(terminal-music-player PRIVATE music-player-application)
music_player_warnings(terminal-music-player)

option(MUSIC_PLAYER_BUILD_AUDIO_SMOKE_TEST
       "Build the manual real-device miniaudio smoke-test executable" OFF)
if(MUSIC_PLAYER_BUILD_AUDIO_SMOKE_TEST)
    add_executable(terminal-music-player-audio-smoke tests/AudioSmoke.cpp)
    target_link_libraries(terminal-music-player-audio-smoke PRIVATE music-player-audio)
    music_player_warnings(terminal-music-player-audio-smoke)
endif()

if(BUILD_TESTING)
    add_executable(terminal-music-player-tests tests/CoreTests.cpp)
    target_link_libraries(terminal-music-player-tests PRIVATE music-player-core)
    music_player_warnings(terminal-music-player-tests)
    add_test(NAME core-tests COMMAND terminal-music-player-tests)

    add_executable(terminal-music-player-app-tests tests/ApplicationTests.cpp)
    target_link_libraries(terminal-music-player-app-tests PRIVATE music-player-application)
    music_player_warnings(terminal-music-player-app-tests)
    add_test(NAME application-tests COMMAND terminal-music-player-app-tests)

    add_executable(terminal-music-player-player-tests tests/PlayerTests.cpp)
    target_link_libraries(terminal-music-player-player-tests PRIVATE music-player-audio)
    music_player_warnings(terminal-music-player-player-tests)
    add_test(NAME player-tests COMMAND terminal-music-player-player-tests)

    set_tests_properties(core-tests application-tests player-tests PROPERTIES TIMEOUT 30)
endif()

cmake_minimum_required(VERSION 3.16)
project(terminal_music_player VERSION 1.0.0 LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

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
    src/InputMapping.cpp
    src/MusicLibrary.cpp
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
)
target_include_directories(music-player-application PUBLIC src)
target_compile_features(music-player-application PUBLIC cxx_std_17)
target_link_libraries(music-player-application PUBLIC music-player-core music-player-audio)
music_player_warnings(music-player-application)

add_executable(terminal-music-player src/main.cpp)
target_compile_features(terminal-music-player PRIVATE cxx_std_17)
target_link_libraries(terminal-music-player PRIVATE music-player-application)
music_player_warnings(terminal-music-player)

if(BUILD_TESTING)
    add_executable(terminal-music-player-tests tests/CoreTests.cpp)
    target_link_libraries(terminal-music-player-tests PRIVATE music-player-core)
    music_player_warnings(terminal-music-player-tests)
    add_test(NAME core-tests COMMAND terminal-music-player-tests)

    add_executable(terminal-music-player-app-tests tests/ApplicationTests.cpp)
    target_link_libraries(terminal-music-player-app-tests PRIVATE music-player-application)
    music_player_warnings(terminal-music-player-app-tests)
    add_test(NAME application-tests COMMAND terminal-music-player-app-tests)
endif()

cmake_minimum_required(VERSION 3.16)
project(terminal_music_player LANGUAGES CXX)

add_executable(terminal-music-player
    src/main.cpp
    src/Application.cpp
    src/ConfigManager.cpp
    src/DataLoader.cpp
    src/MusicLibrary.cpp
    src/Player.cpp
    src/Playlist.cpp
    src/Song.cpp
)

target_compile_features(terminal-music-player PRIVATE cxx_std_17)
find_package(Threads REQUIRED)
target_link_libraries(terminal-music-player PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(terminal-music-player PRIVATE m)
endif()

if(MSVC)
    target_compile_options(terminal-music-player PRIVATE /W4 /permissive-)
else()
    target_compile_options(terminal-music-player PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(terminal-music-player-tests
        tests/CoreTests.cpp
        src/ConfigManager.cpp
        src/DataLoader.cpp
        src/MusicLibrary.cpp
        src/Playlist.cpp
        src/Song.cpp
    )
    target_include_directories(terminal-music-player-tests PRIVATE src)
    target_compile_features(terminal-music-player-tests PRIVATE cxx_std_17)
    add_test(NAME core-tests COMMAND terminal-music-player-tests)
endif()

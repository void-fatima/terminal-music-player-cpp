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

function(configure_audio_target target_name)
    target_link_libraries(${target_name} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(${target_name} PRIVATE ole32 winmm uuid avrt)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${target_name} PRIVATE m)
    endif()
endfunction()

configure_audio_target(terminal-music-player)

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

    add_executable(terminal-music-player-app-tests
        tests/ApplicationTests.cpp
        src/Application.cpp
        src/ConfigManager.cpp
        src/DataLoader.cpp
        src/MusicLibrary.cpp
        src/Player.cpp
        src/Playlist.cpp
        src/Song.cpp
    )
    target_include_directories(terminal-music-player-app-tests PRIVATE src)
    target_compile_features(terminal-music-player-app-tests PRIVATE cxx_std_17)
    configure_audio_target(terminal-music-player-app-tests)
    add_test(NAME application-tests COMMAND terminal-music-player-app-tests)
endif()

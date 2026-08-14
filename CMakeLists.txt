cmake_minimum_required(VERSION 3.16)
project(terminal_music_player LANGUAGES CXX)

add_executable(terminal-music-player
    src/main.cpp
    src/Application.cpp
    src/MusicLibrary.cpp
    src/Song.cpp
)

target_compile_features(terminal-music-player PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(terminal-music-player PRIVATE /W4 /permissive-)
else()
    target_compile_options(terminal-music-player PRIVATE -Wall -Wextra -Wpedantic)
endif()

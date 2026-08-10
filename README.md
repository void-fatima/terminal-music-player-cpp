<div align="center">

# Terminal Music Player

**A keyboard-driven music player that keeps your library, queue, and playback controls inside the terminal.**

[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![Audio](https://img.shields.io/badge/Audio-miniaudio-7C3AED?style=for-the-badge)](https://miniaud.io/)
[![Interface](https://img.shields.io/badge/Interface-Terminal-111827?style=for-the-badge&logo=windowsterminal&logoColor=white)](#features)

<img src="docs/images/terminal-music-player-preview.png" alt="Terminal Music Player final interface preview" width="100%">

<sub>Target product experience — library browsing, queue management, search, playback, and live progress in one focused TUI.</sub>

</div>

## Overview

Terminal Music Player is a C++ CLI/TUI application designed for organizing and playing local audio files directly from the terminal.

The application is structured around a small music library, playlist files, playback state, and a menu-based terminal interface. It uses file-based data sources for music metadata, playlists, and user settings.

## Features

- Load music metadata from a CSV file
- Load playlists from M3U files
- Switch between available playlists
- Play, pause, resume, stop, next, and previous controls
- Playback modes:
  - No repeat
  - Repeat one
  - Repeat all
  - Shuffle
- Browse playlist contents in the terminal
- Search songs by title, artist, or album
- Filter songs by artist or album
- Sort songs by title, artist, album, year, or duration
- Save and restore application settings
- Audio playback through `miniaudio`

## Project Structure

```text
terminal-music-player-cpp/
│
├── src/
│   ├── miniaudio.h
│   └── ...
│
├── Data/
│   ├── library.csv.example
│   ├── settings.cfg
│   ├── Playlists/
│   │   └── example.m3u
│   └── music/
│       └── .gitkeep
│
├── README.md
├── .gitignore
└── .gitattributes
```

## Data Files

### Music Metadata

Music metadata should be stored in:

```text
Data/library.csv
```

Expected CSV format:

```csv
title,artist,album,genre,year,duration_sec,file_path
Bohemian Rhapsody,Queen,A Night at the Opera,Rock,1975,354,Data/music/bohemian.mp3
```

### Playlists

Playlist files should be stored in:

```text
Data/Playlists/
```

Each playlist uses the `.m3u` format and contains one audio file path per line:

```text
Data/music/bohemian.mp3
Data/music/hotel_california.mp3
```

The paths inside playlist files should match the `file_path` values in `Data/library.csv`.

### Settings

Runtime settings can be saved in:

```text
Data/settings.cfg
```

Suggested format:

```text
active_playlist=rock_hits
playback_mode=SHUFFLE
last_song=Data/music/bohemian.mp3
```

## Build

After adding the implementation files, compile the project with a C++17-compatible compiler.

Example using `g++`:

```bash
g++ -std=c++17 src/*.cpp -o terminal-music-player
```

On Linux, depending on the audio backend used by `miniaudio`, you may need to link system audio libraries.

## Run

```bash
./terminal-music-player
```

On Windows:

```bash
terminal-music-player.exe
```

## Suggested Class Design

The application can be organized around these core components:

| Component | Responsibility |
|---|---|
| `Song` | Stores metadata for a single track |
| `Playlist` | Holds an ordered list of songs |
| `MusicLibrary` | Stores all loaded songs and supports search/filter operations |
| `CsvLoader` | Loads song metadata from CSV |
| `M3uLoader` | Loads playlists from M3U files |
| `Player` | Manages playback state and audio controls |
| `ConfigManager` | Loads and saves user settings |
| `Screen` | Base interface for terminal pages |
| `UIRenderer` | Handles terminal output |
| `InputHandler` | Handles and validates user input |
| `Application` | Coordinates the main program flow |

## Development Roadmap

- [ ] Implement `Song` and `Playlist`
- [ ] Implement CSV metadata loading
- [ ] Implement M3U playlist loading
- [ ] Implement music library search and filtering
- [ ] Implement playback state management
- [ ] Integrate `miniaudio`
- [ ] Add terminal screens and menus
- [ ] Add settings persistence
- [ ] Improve input validation and error handling

## Notes

- Real audio files are not included in this repository.
- The `Data/music/` directory is kept with `.gitkeep`, but audio files are ignored by Git.
- `miniaudio.h` is included as a third-party single-header audio library.

---

**Author:** Fatima

# Day 1 — Project Setup & UX Flow

> **Historical document / Concept Flow:** Archived after the production-hardening pass. The current
> implemented interface, controls, ownership, and test guarantees are documented in the repository
> README and `docs/CORE_ARCHITECTURE.md`. Paths and shell behavior below describe the original Day 1
> line-interface concept and are not current product instructions.

Owner: Setayesh  
Duration: 1 day

## Goal

Create a stable project shell and define how a user moves through the terminal music player. Feature modules can be added behind these screens without changing the top-level experience.

## Project structure

```text
music-player/
├── CMakeLists.txt
├── compile.txt
├── docs/
│   └── DAY_01_UX_FLOW.md
├── Data/
│   ├── Playlists/
│   └── music/
└── src/
    ├── main.cpp
    ├── Application.h
    └── Application.cpp
```

`main.cpp` only constructs the application. `Application` owns the top-level screen loop and input/output streams, which keeps navigation testable and leaves music, playlist, playback, and settings logic for their dedicated modules.

## User flow

```text
Start
  |
  v
Main Menu
  |-- Library ------> browse/filter/sort songs ------|
  |-- Playlists ----> select/manage a playlist -----|
  |-- Now Playing --> controls, queue, progress -----|--> Back --> Main Menu
  |-- Search --------> title/artist/album results ---|
  |-- Settings ------> playback/app preferences ----|
  |-- Help ----------> command reference -----------|
  `-- Quit ----------> graceful exit
```

## Interaction rules

- A visible prompt (`>`) means the program is waiting for input.
- Main-menu items accept their number or full English name.
- Commands are case-insensitive and surrounding whitespace is ignored.
- `B`, `Back`, or an empty line returns from a child screen.
- `Q`, `Quit`, or `Exit` closes the application from the main menu.
- Invalid input never terminates the program; it shows an actionable message and asks again.
- The program exits cleanly on end-of-input (for example, Ctrl+Z then Enter on Windows).

## Screen contracts for teammates

| Screen | Input expected from feature module | Primary actions |
|---|---|---|
| Library | `MusicLibrary` song collection | browse, filter, sort, select |
| Playlists | available `Playlist` objects | choose playlist, inspect tracks |
| Now Playing | `Player` state and active queue | play/pause, stop, next, previous |
| Search | search query and library | search by title, artist, album |
| Settings | `ConfigManager` values | change and persist preferences |

## Day-1 acceptance checklist

- [x] C++17 project and repeatable build commands exist.
- [x] Entry point delegates to a top-level application object.
- [x] Main menu exposes every planned user-facing area.
- [x] Navigation, help, back, quit, invalid input, and EOF are handled.
- [x] UX flow and module integration contracts are documented.
- [x] No day-2+ domain or audio implementation is coupled into the shell.

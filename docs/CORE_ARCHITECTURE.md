# Core Architecture

The application separates terminal workflow, metadata, persistence, and audio device lifecycles.

```text
main
  └── Application
      ├── MusicLibrary ── owns ──> Song
      ├── Playlist[] ── references songs by stable ID
      ├── ConfigManager ── reads/writes settings.cfg
      ├── CsvLoader / M3uLoader
      └── Player ── owns queue copies and miniaudio resources
```

## Domain and loading

`Song` is a value object containing a stable numeric ID, metadata, year, duration, and resolved file
path. `MusicLibrary` owns songs and provides case-insensitive search, exact filters, lookup by ID,
and stable sorting.

`CsvLoader` parses the seven-column library format. It accepts quoted commas and escaped quotes,
validates numeric fields, resolves relative paths, and collects non-fatal warnings. `M3uLoader`
discovers playlists in deterministic filename order and maps track paths back to library song IDs.

`Playlist` stores IDs rather than references. Sorting or reallocating the library therefore does not
invalidate playlist membership.

## Playback

`Player` hides miniaudio behind a private implementation so the third-party header does not leak
through the rest of the source tree. It owns copied queue entries, the current index, playback state,
volume, mode, audio engine, and active sound. Audio cleanup follows RAII and happens in the reverse
order of initialization.

Repeat behavior is handled at the queue boundary:

- `NO_REPEAT` stops at the final track.
- `REPEAT_ONE` restarts the completed track.
- `REPEAT_ALL` wraps the queue.
- `SHUFFLE` chooses another queue entry and avoids an immediate duplicate when possible.

The application calls `Player::update()` at interaction boundaries to detect completion and advance
the queue. Device and decoder failures are returned as user-facing errors rather than exceptions.

## Application and persistence

`Application` owns all long-lived components and coordinates screen loops. Input and output streams
are injected, which allows the full menu workflow to be tested without controlling a real terminal.
The data directory is also injectable and can be selected at runtime with `--data-dir`.

`ConfigManager` loads safe defaults when settings are absent or malformed. It writes through a
temporary file before replacing the stored configuration, avoiding partially written settings after
normal I/O failures.

## Tests

`CoreTests.cpp` covers CSV edge cases, M3U matching, search/filter behavior, and settings round trips.
`ApplicationTests.cpp` runs a scripted end-to-end terminal session and verifies navigation, persisted
preferences, playlist selection, and graceful exit. Both are registered with CTest.

# Core Architecture

## Targets and ownership

```text
terminal-music-player executable
  └── music-player-application
      ├── TerminalUi (interactive FTXUI event/render boundary)
      ├── StreamUi (deterministic redirected-input boundary)
      ├── Application (stream-mode compatibility wrapper)
      └── SessionController
          ├── MusicLibrary ── owns Song values and ID/path indexes
          ├── PlaylistManager ── owns editable Playlist values
          ├── ConfigManager ── settings persistence
          └── Player ── queue and playback state machine
              └── IAudioBackend
                  ├── MiniaudioBackend (production device/decoder ownership)
                  └── FakeAudioBackend (test-only deterministic backend)
```

`music-player-core` contains domain types, data loading, indexed library operations, playlist
editing, atomic files, UTF-8 helpers, and CLI parsing. `music-player-audio` contains `Player`, the
backend contract, and miniaudio integration. `music-player-application` contains session
coordination and both interfaces. First-party warning policy is attached to every first-party
target; vendored/fetched dependencies retain their own warning policy.

## Stable song identity and indexes

CSV order is not identity. The loader resolves an identity path relative to the data directory,
lexically removes dot segments, converts separators to `/`, ASCII-case-folds it, and applies the
specified FNV-1a 64 algorithm. It never persists or compares implementation-defined `std::hash`
output.

`MusicLibrary` maintains:

- `unordered_map<Song::Id, size_t>` for expected constant-time ID lookup;
- a normalized resolved-path map for duplicate detection and M3U resolution;
- the owning `vector<Song>` for deterministic iteration and stable sorting.

Indexes are built during insertion, rebuilt after every sort, and cleared with the collection.
Duplicate IDs and normalized paths are rejected. `CsvLoader` separately reports the source line of
duplicates and detects the unlikely case where two distinct identity paths produce the same FNV ID.

## Loading boundaries

`CsvLoader` requires the exact seven-field header, accepts an optional UTF-8 BOM, understands quoted
commas and doubled quotes, and rejects quote syntax such as trailing text after a closing quote.
Records larger than 1 MiB are rejected. Title, path, year, and duration are validated before
insertion. Missing media remains visible metadata with a warning so a user can repair the path.

`M3uLoader` accepts case-insensitive `.m3u`/`.m3u8`, BOMs, comments, and paths relative to each
playlist. Directory and entry operations use error-code overloads. One unreadable file or unmatched
track produces a warning without aborting discovery. Files and entries retain deterministic order.

## Playback state machine

`Player` alone owns queue copies, selected index, playback state, playback mode, volume, shuffle
history, and an `IAudioBackend`. The backend exposes only load/play/pause/stop/seek/volume and
observable timing/completion needed by that state machine.

The interactive ticker sleeps for 200 ms and posts an FTXUI custom event. It never touches session
or player state directly. The UI owner thread consumes that event and calls
`SessionController::tick()`, which calls `Player::update()`. Consequently queue/UI operations and
completion transitions are serialized on one thread; miniaudio owns its internal callback threads
behind `MiniaudioBackend`.

Completion behavior:

- No Repeat stops at the final queue item.
- Repeat One reloads and starts the completed item.
- Repeat All advances and wraps.
- Shuffle chooses a different item when the queue has more than one item, records the prior index,
  and uses that history for Previous.

Manual Next ignores Repeat One semantics: it means move forward, wrapping only for Repeat All and
selecting a new item for Shuffle. Previous restarts the current track when more than three seconds
have elapsed; otherwise it follows shuffle history or sequential order. A failed automatic load or
start leaves the failed item selected, stops the state machine, and exposes the backend error.

Stop rewinds and preserves the queue and selection. Shutdown saves settings, stops the backend, joins
the ticker, and releases sound then engine resources through RAII.

## Playlist and queue editing

`Playlist` stores song IDs, never references or iterators into `MusicLibrary`. `PlaylistManager`
validates filenames, prevents case-insensitive duplicate names, owns CRUD/reorder operations, and
persists each successful edit. Save failures roll an in-memory add/remove/reorder back. M3U output
contains `#EXTM3U` and paths relative to the playlist where possible.

`Player` owns the transient queue. Removing the current item stops first; moving an item adjusts the
selected index; structural edits clear shuffle history because old indices are no longer meaningful.

## Atomic persistence

`AtomicFileWriter` is shared by settings and playlists. The default implementation:

1. creates the parent directory with error-code filesystem operations and removes only matching
   abandoned temporaries older than 24 hours;
2. creates a unique same-directory temporary file using process ID, monotonic time, and an atomic
   process-local counter;
3. writes with an exclusive native handle, flushes (`FlushFileBuffers` or `fsync`), and validates
   close success;
4. atomically replaces through `MoveFileExW(...REPLACE_EXISTING|WRITE_THROUGH)` on Windows or
   `rename(2)` on POSIX;
5. removes only the temporary file when replacement fails.

The last known-good destination is never explicitly deleted. Multiple processes are allowed; each
uses collision-resistant temporaries and the last successful atomic replacement wins. No
multi-process merge or lock is promised. Tests inject `IAtomicFileOps` failures at directory,
temporary-open, write, and replacement boundaries.

## Terminal correctness

FTXUI provides raw cross-platform input, resize-aware layout, viewport framing, terminal capability
detection, Unicode cell measurement, and `NO_COLOR` behavior. The stream UI uses a project helper
that validates UTF-8 boundaries, treats common combining marks as zero-width and CJK/emoji as wide,
and never truncates within a code point. Lists use framed scrolling instead of rendering unbounded
rows into the physical terminal.

## Test boundaries

- `CoreTests.cpp`: stable IDs, 20k-song indexes, sorting, CSV/M3U edge cases, playlist persistence,
  atomic failure injection/concurrency, UTF-8 width, and CLI parsing.
- `PlayerTests.cpp`: backend-independent state transitions, failures, completion, modes, shuffle
  history, queue edits, clamping, and cleanup.
- `ApplicationTests.cpp`: stream commands and session persistence with an injected fake backend.
- `AudioSmoke.cpp`: optional manual real-miniaudio/device exercise; not part of CTest.

Every fixture owns a unique temporary directory and removes only that directory in its destructor,
so repeated and parallel CTest runs do not share names or delete another test's files.

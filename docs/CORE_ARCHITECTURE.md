# Core Music Architecture

The core metadata layer has three focused responsibilities:

```text
Application
    |
    +-- MusicLibrary
            |
            +-- Song
```

## Song

`Song` is a value object for one track's metadata. It stores a numeric ID, title,
artist, album, genre, filesystem path, and millisecond duration. It does not load,
decode, or play audio.

## MusicLibrary

`MusicLibrary` owns the collection of `Song` values. It rejects duplicate song
IDs and provides lookup, case-insensitive metadata search and genre filtering,
and stable sorting by title or artist.

Queries return const references to library-owned songs to avoid unnecessary
copies. Those references should be treated as short-lived views and reacquired
after adding or sorting songs.

## Application

`Application` owns the `MusicLibrary` and remains responsible for top-level
terminal navigation and program flow. This step does not change the existing
screens or implement the full library interface.

## Playback boundary

Playback is intentionally separate from both metadata classes. Audio device
management, decoding, playback state, and queue behavior have different
lifecycles and will belong to later components. Keeping that work outside
`Song` and `MusicLibrary` lets the metadata layer remain simple and testable.

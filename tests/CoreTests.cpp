#include "ConfigManager.h"
#include "DataLoader.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace music_player;

    const auto directory = fs::temp_directory_path() / "terminal-music-player-tests";
    std::error_code error;
    fs::remove_all(directory, error);
    fs::create_directories(directory / "Playlists", error);

    {
        std::ofstream csv(directory / "library.csv");
        csv << "title,artist,album,genre,year,duration_sec,file_path\n"
            << "\"A, Song\",Artist,Album,Rock,2024,125,music/a.mp3\n"
            << "Bad,Row\n"
            << "Second,Another,Record,Jazz,2020,60,music/b.mp3\n";
    }

    MusicLibrary library;
    const auto report = CsvLoader::load(directory / "library.csv", library);
    expect(report.loaded == 2, "CSV loads valid rows");
    expect(report.warnings.size() == 1, "CSV reports malformed rows");
    expect(library.songs().front().title() == "A, Song", "CSV handles quoted commas");
    expect(library.songs().front().year() == 2024, "CSV loads year");
    expect(library.search("artist").size() == 1, "global search is case-insensitive");
    expect(library.filterByGenre("ROCK").size() == 1, "genre filter is case-insensitive");

    {
        std::ofstream playlist(directory / "Playlists" / "favorites.m3u");
        playlist << "../music/a.mp3\n# comment\n../music/missing.mp3\n";
    }
    std::vector<Playlist> playlists;
    const auto playlistReport = M3uLoader::loadDirectory(directory / "Playlists", library, playlists);
    expect(playlistReport.loaded == 1, "M3U playlist is discovered");
    expect(playlists.size() == 1 && playlists.front().songIds().size() == 1,
           "M3U resolves tracks in the library");

    ConfigManager config(directory / "settings.cfg");
    AppSettings settings;
    settings.activePlaylist = "favorites";
    settings.playbackMode = "SHUFFLE";
    settings.lastSong = "music/a.mp3";
    settings.volume = 0.35F;
    std::string saveError;
    expect(config.save(settings, saveError), "settings are saved");
    const auto loaded = config.load();
    expect(loaded.activePlaylist == "favorites", "active playlist is restored");
    expect(loaded.playbackMode == "SHUFFLE", "playback mode is restored");
    expect(loaded.volume > 0.34F && loaded.volume < 0.36F, "volume is restored");

    fs::remove_all(directory, error);
    if (failures == 0) std::cout << "All core tests passed.\n";
    return failures == 0 ? 0 : 1;
}

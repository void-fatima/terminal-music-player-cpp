#include "Application.h"
#include "ConfigManager.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

    const auto data = fs::temp_directory_path() / "terminal-music-player-app-tests";
    std::error_code error;
    fs::remove_all(data, error);
    fs::create_directories(data / "Playlists", error);
    {
        std::ofstream csv(data / "library.csv");
        csv << "title,artist,album,genre,year,duration_sec,file_path\n"
            << "First Track,Example Artist,Example Album,Rock,2025,90,music/first.wav\n";
        std::ofstream playlist(data / "Playlists" / "favorites.m3u");
        playlist << "../music/first.wav\n";
    }

    AppSettings initialSettings;
    initialSettings.lastSong = (data / "music" / "first.wav").generic_string();
    std::string settingsError;
    expect(ConfigManager(data / "settings.cfg").save(initialSettings, settingsError),
           "initial session is saved");

    std::istringstream input{
        "3\nb\n"
        "1\nsort year\nfilter genre rock\nb\n"
        "2\n1\n\n"
        "4\nFirst\n\n"
        "5\nvolume 40\nmode repeat-all\nb\n"
        "h\ninvalid\nb\nq\n"};
    std::ostringstream output;
    Application application{input, output, data};
    expect(application.run() == 0, "application exits successfully");

    const auto text = output.str();
    expect(text.find("Loaded 1 songs and 1 playlists") != std::string::npos,
           "startup reports loaded content");
    expect(text.find("First Track") != std::string::npos, "library and search display a track");
    expect(text.find("Queue: 1/1") != std::string::npos, "last track is restored into the queue");
    expect(text.find("favorites") != std::string::npos, "playlist is displayed");
    expect(text.find("Enter B to return") != std::string::npos, "help rejects invalid back commands");
    expect(text.find("Goodbye") != std::string::npos, "application exits gracefully");

    const auto settings = ConfigManager(data / "settings.cfg").load();
    expect(settings.activePlaylist == "favorites", "active playlist persists");
    expect(settings.playbackMode == "REPEAT_ALL", "playback mode persists");
    expect(settings.volume > 0.39F && settings.volume < 0.41F, "volume persists");

    fs::remove_all(data, error);
    if (failures == 0) std::cout << "All application tests passed.\n";
    return failures == 0 ? 0 : 1;
}

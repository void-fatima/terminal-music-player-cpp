#pragma once

#include "ConfigManager.h"
#include "MusicLibrary.h"
#include "Player.h"
#include "Playlist.h"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace music_player {

class Application {
public:
    Application(std::istream& input,
                std::ostream& output,
                std::filesystem::path dataDirectory = "Data");
    int run();

private:
    enum class Screen { MainMenu, Library, Playlists, NowPlaying, Search, Settings, Help, Exit };
    using SongView = std::vector<std::reference_wrapper<const Song>>;

    void loadData();
    void saveSettings();
    void renderHeader() const;
    void renderMainMenu() const;
    void renderHelp() const;
    void runLibrary();
    void runPlaylists();
    void runNowPlaying();
    void runSearch();
    void runSettings();
    void dispatchMainMenu(const std::string& command);
    void waitForBack();

    SongView allSongs() const;
    SongView playlistSongs(const Playlist& playlist) const;
    void renderSongs(const SongView& songs) const;
    bool playSelection(const SongView& songs, const std::string& argument);
    std::optional<std::string> prompt(const std::string& label);

    static std::string normalize(std::string value);
    static std::string trim(std::string value);
    static std::string formatDuration(double seconds);
    static std::string fit(std::string value, std::size_t width);
    static std::optional<std::size_t> parseSelection(const std::string& value, std::size_t count);

    std::istream& input_;
    std::ostream& output_;
    std::filesystem::path dataDirectory_;
    ConfigManager config_;
    MusicLibrary library_;
    std::vector<Playlist> playlists_;
    Player player_;
    AppSettings settings_;
    std::optional<std::size_t> activePlaylist_;
    std::vector<std::string> loadWarnings_;
    Screen currentScreen_{Screen::MainMenu};
};

}  // namespace music_player

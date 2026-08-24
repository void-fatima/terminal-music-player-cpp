#pragma once

#include "ConfigManager.h"
#include "MusicLibrary.h"
#include "Player.h"
#include "PlaylistManager.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace music_player {

class SessionController {
public:
    explicit SessionController(std::filesystem::path dataDirectory,
                               std::unique_ptr<IAudioBackend> audioBackend = {});
    ~SessionController();

    bool load();
    bool reload();
    bool tick();
    bool shutdown();

    const std::filesystem::path& dataDirectory() const noexcept;
    const MusicLibrary& library() const noexcept;
    MusicLibrary& library() noexcept;
    const std::vector<Playlist>& playlists() const noexcept;
    PlaylistManager& playlistManager() noexcept;
    Player& player() noexcept;
    const Player& player() const noexcept;
    const std::vector<std::string>& warnings() const noexcept;
    const std::string& message() const noexcept;
    bool messageIsError() const noexcept;

    std::optional<std::size_t> activePlaylist() const noexcept;
    void setActivePlaylist(std::optional<std::size_t> index);
    std::vector<Song> playlistSongs(std::size_t index) const;

    bool playLibrary(std::size_t index);
    bool playPlaylist(std::size_t playlistIndex, std::size_t trackIndex);
    bool enqueueLibrary(std::size_t index);
    bool enqueuePlaylist(std::size_t index);
    bool setVolume(float volume);
    void setMode(PlaybackMode mode);
    void setMessage(std::string message, bool isError = false);
    bool saveSettings();

private:
    std::filesystem::path dataDirectory_;
    ConfigManager config_;
    MusicLibrary library_;
    PlaylistManager playlistManager_;
    Player player_;
    AppSettings settings_;
    std::optional<std::size_t> activePlaylist_;
    std::vector<std::string> warnings_;
    std::string message_;
    bool messageIsError_{false};
    bool loaded_{false};
    bool shutdown_{false};
};

}  // namespace music_player

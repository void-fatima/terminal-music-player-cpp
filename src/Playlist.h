#pragma once

#include "Song.h"

#include <filesystem>
#include <string>
#include <vector>

namespace music_player {

class Playlist {
public:
    Playlist(std::string name, std::filesystem::path sourcePath, std::vector<Song::Id> songIds);

    const std::string& name() const noexcept;
    const std::filesystem::path& sourcePath() const noexcept;
    const std::vector<Song::Id>& songIds() const noexcept;

    void rename(std::string name, std::filesystem::path sourcePath);
    void addSong(Song::Id id);
    bool removeSong(std::size_t index);
    bool moveSong(std::size_t from, std::size_t to);

private:
    std::string name_;
    std::filesystem::path sourcePath_;
    std::vector<Song::Id> songIds_;
};

}  // namespace music_player

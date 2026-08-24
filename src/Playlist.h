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

private:
    std::string name_;
    std::filesystem::path sourcePath_;
    std::vector<Song::Id> songIds_;
};

}  // namespace music_player

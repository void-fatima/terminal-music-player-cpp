#include "Playlist.h"

#include <utility>

namespace music_player {

Playlist::Playlist(std::string name, std::filesystem::path sourcePath, std::vector<Song::Id> songIds)
    : name_(std::move(name)), sourcePath_(std::move(sourcePath)), songIds_(std::move(songIds)) {}

const std::string& Playlist::name() const noexcept { return name_; }
const std::filesystem::path& Playlist::sourcePath() const noexcept { return sourcePath_; }
const std::vector<Song::Id>& Playlist::songIds() const noexcept { return songIds_; }

}  // namespace music_player

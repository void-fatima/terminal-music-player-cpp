#include "Playlist.h"

#include <cstddef>
#include <utility>

namespace music_player {

Playlist::Playlist(std::string name, std::filesystem::path sourcePath, std::vector<Song::Id> songIds)
    : name_(std::move(name)), sourcePath_(std::move(sourcePath)), songIds_(std::move(songIds)) {}

const std::string& Playlist::name() const noexcept { return name_; }
const std::filesystem::path& Playlist::sourcePath() const noexcept { return sourcePath_; }
const std::vector<Song::Id>& Playlist::songIds() const noexcept { return songIds_; }

void Playlist::rename(std::string name, std::filesystem::path sourcePath) {
    name_ = std::move(name);
    sourcePath_ = std::move(sourcePath);
}

void Playlist::addSong(Song::Id id) { songIds_.push_back(id); }

bool Playlist::removeSong(std::size_t index) {
    if (index >= songIds_.size()) return false;
    songIds_.erase(songIds_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool Playlist::moveSong(std::size_t from, std::size_t to) {
    if (from >= songIds_.size() || to >= songIds_.size()) return false;
    if (from == to) return true;
    const Song::Id moving = songIds_[from];
    songIds_.erase(songIds_.begin() + static_cast<std::ptrdiff_t>(from));
    songIds_.insert(songIds_.begin() + static_cast<std::ptrdiff_t>(to), moving);
    return true;
}

}  // namespace music_player

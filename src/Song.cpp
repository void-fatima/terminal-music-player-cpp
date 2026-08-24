#include "Song.h"

#include <utility>

namespace music_player {

Song::Song(Id id,
           std::string title,
           std::string artist,
           std::string album,
           std::string genre,
           std::filesystem::path filePath,
           Duration duration,
           int year)
    : id_(id),
      title_(std::move(title)),
      artist_(std::move(artist)),
      album_(std::move(album)),
      genre_(std::move(genre)),
      filePath_(std::move(filePath)),
      duration_(duration),
      year_(year) {}

Song::Id Song::id() const noexcept {
    return id_;
}

const std::string& Song::title() const noexcept {
    return title_;
}

const std::string& Song::artist() const noexcept {
    return artist_;
}

const std::string& Song::album() const noexcept {
    return album_;
}

const std::string& Song::genre() const noexcept {
    return genre_;
}

const std::filesystem::path& Song::filePath() const noexcept {
    return filePath_;
}

Song::Duration Song::duration() const noexcept {
    return duration_;
}

int Song::year() const noexcept {
    return year_;
}

}  // namespace music_player

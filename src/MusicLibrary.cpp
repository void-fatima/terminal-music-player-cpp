#include "MusicLibrary.h"

#include "StableId.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace music_player {
namespace {

char toLower(char character) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
}

bool equalsCaseInsensitive(std::string_view left, std::string_view right) {
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(), [](char lhs, char rhs) {
               return toLower(lhs) == toLower(rhs);
           });
}

bool containsCaseInsensitive(std::string_view value, std::string_view query) {
    return std::search(value.begin(), value.end(), query.begin(), query.end(),
                       [](char lhs, char rhs) { return toLower(lhs) == toLower(rhs); })
        != value.end();
}

bool lessCaseInsensitive(std::string_view left, std::string_view right) {
    return std::lexicographical_compare(
        left.begin(), left.end(), right.begin(), right.end(),
        [](char lhs, char rhs) { return toLower(lhs) < toLower(rhs); });
}

template <typename FieldAccessor>
MusicLibrary::SearchResults searchSongs(const std::vector<Song>& songs,
                                        std::string_view query,
                                        FieldAccessor field) {
    MusicLibrary::SearchResults results;
    for (const Song& song : songs) {
        if (containsCaseInsensitive(field(song), query)) {
            results.emplace_back(song);
        }
    }
    return results;
}

}  // namespace

bool MusicLibrary::addSong(Song song) {
    const auto path = normalizedPathKey(song.filePath());
    if (idIndex_.find(song.id()) != idIndex_.end() || pathIndex_.find(path) != pathIndex_.end()) {
        return false;
    }
    const auto index = songs_.size();
    const auto id = song.id();
    songs_.push_back(std::move(song));
    idIndex_.emplace(id, index);
    pathIndex_.emplace(path, id);
    return true;
}

const std::vector<Song>& MusicLibrary::songs() const noexcept {
    return songs_;
}

std::optional<std::reference_wrapper<const Song>> MusicLibrary::findById(Song::Id id) const noexcept {
    const auto found = idIndex_.find(id);
    return found == idIndex_.end() ? std::nullopt : std::optional{std::cref(songs_[found->second])};
}

std::optional<std::reference_wrapper<const Song>> MusicLibrary::findByPath(
    const std::filesystem::path& path) const {
    const auto found = pathIndex_.find(normalizedPathKey(path));
    return found == pathIndex_.end() ? std::nullopt : findById(found->second);
}

bool MusicLibrary::containsPath(const std::filesystem::path& path) const {
    return pathIndex_.find(normalizedPathKey(path)) != pathIndex_.end();
}

MusicLibrary::SearchResults MusicLibrary::searchByTitle(std::string_view query) const {
    return searchSongs(songs_, query, [](const Song& song) -> const std::string& {
        return song.title();
    });
}

MusicLibrary::SearchResults MusicLibrary::searchByArtist(std::string_view query) const {
    return searchSongs(songs_, query, [](const Song& song) -> const std::string& {
        return song.artist();
    });
}

MusicLibrary::SearchResults MusicLibrary::searchByAlbum(std::string_view query) const {
    return searchSongs(songs_, query, [](const Song& song) -> const std::string& {
        return song.album();
    });
}

MusicLibrary::SearchResults MusicLibrary::search(std::string_view query) const {
    SearchResults results;
    for (const Song& song : songs_) {
        if (containsCaseInsensitive(song.title(), query)
            || containsCaseInsensitive(song.artist(), query)
            || containsCaseInsensitive(song.album(), query)) {
            results.emplace_back(song);
        }
    }
    return results;
}

MusicLibrary::SearchResults MusicLibrary::filterByArtist(std::string_view artist) const {
    MusicLibrary::SearchResults results;
    for (const Song& song : songs_) {
        if (equalsCaseInsensitive(song.artist(), artist)) {
            results.emplace_back(song);
        }
    }
    return results;
}

MusicLibrary::SearchResults MusicLibrary::filterByAlbum(std::string_view album) const {
    MusicLibrary::SearchResults results;
    for (const Song& song : songs_) {
        if (equalsCaseInsensitive(song.album(), album)) {
            results.emplace_back(song);
        }
    }
    return results;
}

MusicLibrary::SearchResults MusicLibrary::filterByGenre(std::string_view genre) const {
    MusicLibrary::SearchResults results;
    for (const Song& song : songs_) {
        if (equalsCaseInsensitive(song.genre(), genre)) {
            results.emplace_back(song);
        }
    }
    return results;
}

void MusicLibrary::sortByTitle() {
    std::stable_sort(songs_.begin(), songs_.end(), [](const Song& left, const Song& right) {
        return lessCaseInsensitive(left.title(), right.title());
    });
    rebuildIndexes();
}

void MusicLibrary::sortByArtist() {
    std::stable_sort(songs_.begin(), songs_.end(), [](const Song& left, const Song& right) {
        return lessCaseInsensitive(left.artist(), right.artist());
    });
    rebuildIndexes();
}

void MusicLibrary::sortByAlbum() {
    std::stable_sort(songs_.begin(), songs_.end(), [](const Song& left, const Song& right) {
        return lessCaseInsensitive(left.album(), right.album());
    });
    rebuildIndexes();
}

void MusicLibrary::sortByYear() {
    std::stable_sort(songs_.begin(), songs_.end(), [](const Song& left, const Song& right) {
        return left.year() < right.year();
    });
    rebuildIndexes();
}

void MusicLibrary::sortByDuration() {
    std::stable_sort(songs_.begin(), songs_.end(), [](const Song& left, const Song& right) {
        return left.duration() < right.duration();
    });
    rebuildIndexes();
}

void MusicLibrary::clear() noexcept {
    songs_.clear();
    idIndex_.clear();
    pathIndex_.clear();
}

void MusicLibrary::rebuildIndexes() {
    idIndex_.clear();
    pathIndex_.clear();
    idIndex_.reserve(songs_.size());
    pathIndex_.reserve(songs_.size());
    for (std::size_t index = 0; index < songs_.size(); ++index) {
        idIndex_.emplace(songs_[index].id(), index);
        pathIndex_.emplace(normalizedPathKey(songs_[index].filePath()), songs_[index].id());
    }
}

}  // namespace music_player

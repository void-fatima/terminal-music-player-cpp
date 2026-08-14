#include "MusicLibrary.h"

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
    if (findById(song.id())) {
        return false;
    }

    songs_.push_back(std::move(song));
    return true;
}

const std::vector<Song>& MusicLibrary::songs() const noexcept {
    return songs_;
}

std::optional<std::reference_wrapper<const Song>> MusicLibrary::findById(Song::Id id) const noexcept {
    const auto song = std::find_if(songs_.begin(), songs_.end(),
                                   [id](const Song& candidate) { return candidate.id() == id; });
    if (song == songs_.end()) {
        return std::nullopt;
    }
    return std::cref(*song);
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
}

void MusicLibrary::sortByArtist() {
    std::stable_sort(songs_.begin(), songs_.end(), [](const Song& left, const Song& right) {
        return lessCaseInsensitive(left.artist(), right.artist());
    });
}

}  // namespace music_player

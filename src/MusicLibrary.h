#pragma once

#include "Song.h"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace music_player {

class MusicLibrary {
public:
    using SearchResults = std::vector<std::reference_wrapper<const Song>>;

    bool addSong(Song song);

    const std::vector<Song>& songs() const noexcept;
    std::optional<std::reference_wrapper<const Song>> findById(Song::Id id) const noexcept;

    SearchResults searchByTitle(std::string_view query) const;
    SearchResults searchByArtist(std::string_view query) const;
    SearchResults searchByAlbum(std::string_view query) const;
    SearchResults filterByGenre(std::string_view genre) const;

    void sortByTitle();
    void sortByArtist();

private:
    std::vector<Song> songs_;
};

}  // namespace music_player

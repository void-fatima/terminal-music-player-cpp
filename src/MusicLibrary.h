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
    SearchResults search(std::string_view query) const;
    SearchResults filterByArtist(std::string_view artist) const;
    SearchResults filterByAlbum(std::string_view album) const;
    SearchResults filterByGenre(std::string_view genre) const;

    void sortByTitle();
    void sortByArtist();
    void sortByAlbum();
    void sortByYear();
    void sortByDuration();
    void clear() noexcept;

private:
    std::vector<Song> songs_;
};

}  // namespace music_player

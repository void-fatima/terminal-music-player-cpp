#pragma once

#include "Song.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace music_player {

class MusicLibrary {
public:
    using SearchResults = std::vector<std::reference_wrapper<const Song>>;

    bool addSong(Song song);

    const std::vector<Song>& songs() const noexcept;
    std::optional<std::reference_wrapper<const Song>> findById(Song::Id id) const noexcept;
    std::optional<std::reference_wrapper<const Song>> findByPath(
        const std::filesystem::path& path) const;
    bool containsPath(const std::filesystem::path& path) const;

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
    void rebuildIndexes();

    std::vector<Song> songs_;
    std::unordered_map<Song::Id, std::size_t> idIndex_;
    std::unordered_map<std::string, Song::Id> pathIndex_;
};

}  // namespace music_player

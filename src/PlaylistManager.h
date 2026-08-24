#pragma once

#include "AtomicFile.h"
#include "DataLoader.h"
#include "MusicLibrary.h"
#include "Playlist.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace music_player {

class PlaylistManager {
public:
    PlaylistManager(std::filesystem::path directory,
                    const MusicLibrary& library,
                    std::shared_ptr<IAtomicFileOps> operations = {});

    LoadReport reload();
    const std::vector<Playlist>& playlists() const noexcept;
    std::vector<Playlist>& playlists() noexcept;

    bool create(const std::string& name, std::string& error);
    bool rename(std::size_t index, const std::string& name, std::string& error);
    bool erase(std::size_t index, std::string& error);
    bool addTrack(std::size_t playlistIndex, Song::Id id, std::string& error);
    bool removeTrack(std::size_t playlistIndex, std::size_t trackIndex, std::string& error);
    bool moveTrack(std::size_t playlistIndex,
                   std::size_t from,
                   std::size_t to,
                   std::string& error);
    bool save(std::size_t index, std::string& error) const;

    static bool validName(const std::string& name, std::string& error);

private:
    bool nameAvailable(const std::string& name, std::size_t ignoredIndex) const;
    std::string serialize(const Playlist& playlist) const;

    std::filesystem::path directory_;
    const MusicLibrary& library_;
    AtomicFileWriter writer_;
    std::vector<Playlist> playlists_;
};

}  // namespace music_player

#pragma once

#include "MusicLibrary.h"
#include "Playlist.h"

#include <filesystem>
#include <string>
#include <vector>

namespace music_player {

struct LoadReport {
    std::size_t loaded{0};
    std::vector<std::string> warnings;
};

class CsvLoader {
public:
    static LoadReport load(const std::filesystem::path& file, MusicLibrary& library);
};

class M3uLoader {
public:
    static LoadReport loadDirectory(const std::filesystem::path& directory,
                                    const MusicLibrary& library,
                                    std::vector<Playlist>& playlists);
};

}  // namespace music_player

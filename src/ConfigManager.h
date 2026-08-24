#pragma once

#include "AtomicFile.h"

#include <filesystem>
#include <memory>
#include <string>

namespace music_player {

struct AppSettings {
    std::string activePlaylist;
    std::string playbackMode{"NO_REPEAT"};
    std::string lastSong;
    float volume{0.8F};
};

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path file,
                           std::shared_ptr<IAtomicFileOps> operations = {});

    AppSettings load() const;
    bool save(const AppSettings& settings, std::string& error) const;

private:
    std::filesystem::path file_;
    AtomicFileWriter writer_;
};

}  // namespace music_player

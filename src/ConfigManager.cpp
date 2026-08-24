#include "ConfigManager.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace music_player {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string cleanValue(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());
    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
    return value;
}

}  // namespace

ConfigManager::ConfigManager(std::filesystem::path file) : file_(std::move(file)) {}

AppSettings ConfigManager::load() const {
    AppSettings settings;
    std::ifstream input(file_);
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if (key == "active_playlist") settings.activePlaylist = value;
        else if (key == "playback_mode") settings.playbackMode = value;
        else if (key == "last_song") settings.lastSong = value;
        else if (key == "volume") {
            try {
                std::size_t consumed = 0;
                const float parsed = std::stof(value, &consumed);
                if (consumed == value.size() && parsed >= 0.0F && parsed <= 1.0F) settings.volume = parsed;
            } catch (...) {
                // Keep the safe default for malformed user configuration.
            }
        }
    }
    return settings;
}

bool ConfigManager::save(const AppSettings& settings, std::string& error) const {
    std::error_code filesystemError;
    std::filesystem::create_directories(file_.parent_path(), filesystemError);
    const auto temporary = file_.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "Cannot write settings file: " + temporary;
        return false;
    }
    output << "# Terminal Music Player settings\n"
           << "active_playlist=" << cleanValue(settings.activePlaylist) << '\n'
           << "playback_mode=" << cleanValue(settings.playbackMode) << '\n'
           << "last_song=" << cleanValue(settings.lastSong) << '\n'
           << "volume=" << std::fixed << std::setprecision(2)
           << std::clamp(settings.volume, 0.0F, 1.0F) << '\n';
    output.close();
    if (!output) {
        error = "Failed while writing settings file: " + temporary;
        return false;
    }
    std::filesystem::remove(file_, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporary, file_, filesystemError);
    if (filesystemError) {
        error = "Cannot replace settings file: " + filesystemError.message();
        return false;
    }
    return true;
}

}  // namespace music_player

#include "PlaylistManager.h"

#include "StableId.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <system_error>

namespace music_player {
namespace {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

}  // namespace

PlaylistManager::PlaylistManager(std::filesystem::path directory,
                                 const MusicLibrary& library,
                                 std::shared_ptr<IAtomicFileOps> operations)
    : directory_(std::move(directory)), library_(library), writer_(std::move(operations)) {}

LoadReport PlaylistManager::reload() {
    return M3uLoader::loadDirectory(directory_, library_, playlists_);
}

const std::vector<Playlist>& PlaylistManager::playlists() const noexcept { return playlists_; }
std::vector<Playlist>& PlaylistManager::playlists() noexcept { return playlists_; }

bool PlaylistManager::validName(const std::string& name, std::string& error) {
    error.clear();
    if (name.empty() || name == "." || name == "..") {
        error = "Playlist name cannot be empty, '.' or '..'.";
        return false;
    }
    if (name.back() == ' ' || name.back() == '.') {
        error = "Playlist name cannot end with a space or period.";
        return false;
    }
    constexpr const char* invalid = "<>:\"/\\|?*";
    for (const unsigned char character : name) {
        if (character < 0x20 || std::string(invalid).find(static_cast<char>(character)) != std::string::npos) {
            error = "Playlist name contains a character that is not valid in a filename.";
            return false;
        }
    }
    return true;
}

bool PlaylistManager::nameAvailable(const std::string& name, std::size_t ignoredIndex) const {
    const auto candidate = lowerAscii(name);
    for (std::size_t index = 0; index < playlists_.size(); ++index) {
        if (index != ignoredIndex && lowerAscii(playlists_[index].name()) == candidate) return false;
    }
    return true;
}

bool PlaylistManager::create(const std::string& name, std::string& error) {
    if (!validName(name, error)) return false;
    if (!nameAvailable(name, std::numeric_limits<std::size_t>::max())) {
        error = "A playlist named '" + name + "' already exists.";
        return false;
    }
    Playlist playlist{name, directory_ / (name + ".m3u8"), {}};
    if (!writer_.write(playlist.sourcePath(), serialize(playlist), error)) return false;
    playlists_.push_back(std::move(playlist));
    return true;
}

bool PlaylistManager::rename(std::size_t index, const std::string& name, std::string& error) {
    if (index >= playlists_.size()) {
        error = "Playlist index is out of range.";
        return false;
    }
    if (!validName(name, error)) return false;
    if (!nameAvailable(name, index)) {
        error = "A playlist named '" + name + "' already exists.";
        return false;
    }
    const auto oldPath = playlists_[index].sourcePath();
    const auto extension = lowerAscii(oldPath.extension().string()) == ".m3u" ? ".m3u" : ".m3u8";
    const auto newPath = directory_ / (name + extension);
    Playlist replacement{name, newPath, playlists_[index].songIds()};
    if (oldPath != newPath && normalizedPathKey(oldPath) == normalizedPathKey(newPath)) {
        std::error_code filesystemError;
        std::filesystem::rename(oldPath, newPath, filesystemError);
        if (filesystemError) {
            error = "Cannot change playlist filename casing: " + filesystemError.message();
            return false;
        }
        playlists_[index] = std::move(replacement);
        return true;
    }
    if (!writer_.write(newPath, serialize(replacement), error)) return false;
    if (oldPath != newPath) {
        std::error_code filesystemError;
        if (!std::filesystem::remove(oldPath, filesystemError) || filesystemError) {
            std::error_code ignored;
            std::filesystem::remove(newPath, ignored);
            error = "New playlist was written, but the old file could not be removed: "
                + (filesystemError ? filesystemError.message() : "file not found");
            return false;
        }
    }
    playlists_[index] = std::move(replacement);
    return true;
}

bool PlaylistManager::erase(std::size_t index, std::string& error) {
    if (index >= playlists_.size()) {
        error = "Playlist index is out of range.";
        return false;
    }
    std::error_code filesystemError;
    if (!std::filesystem::remove(playlists_[index].sourcePath(), filesystemError) || filesystemError) {
        error = "Cannot delete playlist file: "
            + (filesystemError ? filesystemError.message() : "file not found");
        return false;
    }
    playlists_.erase(playlists_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool PlaylistManager::addTrack(std::size_t playlistIndex, Song::Id id, std::string& error) {
    if (playlistIndex >= playlists_.size() || !library_.findById(id)) {
        error = "Playlist or song does not exist.";
        return false;
    }
    playlists_[playlistIndex].addSong(id);
    if (save(playlistIndex, error)) return true;
    (void)playlists_[playlistIndex].removeSong(playlists_[playlistIndex].songIds().size() - 1);
    return false;
}

bool PlaylistManager::removeTrack(std::size_t playlistIndex,
                                  std::size_t trackIndex,
                                  std::string& error) {
    if (playlistIndex >= playlists_.size() || trackIndex >= playlists_[playlistIndex].songIds().size()) {
        error = "Playlist track index is out of range.";
        return false;
    }
    const auto previous = playlists_[playlistIndex].songIds();
    (void)playlists_[playlistIndex].removeSong(trackIndex);
    if (save(playlistIndex, error)) return true;
    playlists_[playlistIndex] = Playlist{playlists_[playlistIndex].name(),
                                         playlists_[playlistIndex].sourcePath(), previous};
    return false;
}

bool PlaylistManager::moveTrack(std::size_t playlistIndex,
                                std::size_t from,
                                std::size_t to,
                                std::string& error) {
    if (playlistIndex >= playlists_.size() || !playlists_[playlistIndex].moveSong(from, to)) {
        error = "Playlist track index is out of range.";
        return false;
    }
    if (save(playlistIndex, error)) return true;
    (void)playlists_[playlistIndex].moveSong(to, from);
    return false;
}

bool PlaylistManager::save(std::size_t index, std::string& error) const {
    if (index >= playlists_.size()) {
        error = "Playlist index is out of range.";
        return false;
    }
    return writer_.write(playlists_[index].sourcePath(), serialize(playlists_[index]), error);
}

std::string PlaylistManager::serialize(const Playlist& playlist) const {
    std::ostringstream output;
    output << "#EXTM3U\n";
    for (const Song::Id id : playlist.songIds()) {
        const auto song = library_.findById(id);
        if (!song) continue;
        auto path = song->get().filePath().lexically_relative(playlist.sourcePath().parent_path());
        if (path.empty()) path = song->get().filePath();
        output << path.generic_string() << '\n';
    }
    return output.str();
}

}  // namespace music_player

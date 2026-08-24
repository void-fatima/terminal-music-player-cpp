#include "SessionController.h"

#include "DataLoader.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace music_player {

SessionController::SessionController(std::filesystem::path dataDirectory,
                                     std::unique_ptr<IAudioBackend> audioBackend)
    : dataDirectory_(std::move(dataDirectory)),
      config_(dataDirectory_ / "settings.cfg"),
      playlistManager_(dataDirectory_ / "Playlists", library_),
      player_(std::move(audioBackend)) {}

SessionController::~SessionController() {
    if (!shutdown_) (void)player_.stop();
}

bool SessionController::load() {
    warnings_.clear();
    message_.clear();
    messageIsError_ = false;
    player_.clearQueue();
    library_.clear();

    auto libraryFile = dataDirectory_ / "library.csv";
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(libraryFile, filesystemError)) {
        libraryFile = dataDirectory_ / "library.csv.example";
    }
    auto report = CsvLoader::load(libraryFile, library_);
    warnings_.insert(warnings_.end(), report.warnings.begin(), report.warnings.end());
    report = playlistManager_.reload();
    warnings_.insert(warnings_.end(), report.warnings.begin(), report.warnings.end());

    settings_ = config_.load();
    player_.setMode(Player::parseMode(settings_.playbackMode));
    if (!player_.setVolume(settings_.volume) && player_.audioAvailable()) {
        warnings_.push_back(player_.lastError());
    }

    activePlaylist_.reset();
    for (std::size_t index = 0; index < playlists().size(); ++index) {
        if (playlists()[index].name() == settings_.activePlaylist) {
            activePlaylist_ = index;
            break;
        }
    }

    if (!settings_.lastSong.empty()) {
        std::vector<Song> queue = library_.songs();
        const auto restored = std::find_if(queue.begin(), queue.end(), [this](const Song& song) {
            return song.filePath().generic_string() == settings_.lastSong;
        });
        if (restored != queue.end()) {
            const auto index = static_cast<std::size_t>(restored - queue.begin());
            (void)player_.prepare(std::move(queue), index);
        }
    }

    loaded_ = true;
    shutdown_ = false;
    message_ = "Loaded " + std::to_string(library_.songs().size()) + " songs and "
        + std::to_string(playlists().size()) + " playlists.";
    return true;
}

bool SessionController::reload() {
    const bool result = load();
    if (result) setMessage("Library and playlists reloaded.");
    return result;
}

bool SessionController::tick() {
    if (player_.update()) return true;
    if (!player_.lastError().empty()) {
        setMessage("Playback transition failed: " + player_.lastError(), true);
        return false;
    }
    return true;
}

bool SessionController::shutdown() {
    if (shutdown_) return true;
    bool success = saveSettings();
    if (!player_.stop()) {
        setMessage(player_.lastError(), true);
        success = false;
    }
    shutdown_ = true;
    return success;
}

const std::filesystem::path& SessionController::dataDirectory() const noexcept { return dataDirectory_; }
const MusicLibrary& SessionController::library() const noexcept { return library_; }
MusicLibrary& SessionController::library() noexcept { return library_; }
const std::vector<Playlist>& SessionController::playlists() const noexcept {
    return playlistManager_.playlists();
}
PlaylistManager& SessionController::playlistManager() noexcept { return playlistManager_; }
Player& SessionController::player() noexcept { return player_; }
const Player& SessionController::player() const noexcept { return player_; }
const std::vector<std::string>& SessionController::warnings() const noexcept { return warnings_; }
const std::string& SessionController::message() const noexcept { return message_; }
bool SessionController::messageIsError() const noexcept { return messageIsError_; }
std::optional<std::size_t> SessionController::activePlaylist() const noexcept { return activePlaylist_; }

void SessionController::setActivePlaylist(std::optional<std::size_t> index) {
    activePlaylist_ = index && *index < playlists().size() ? index : std::nullopt;
    (void)saveSettings();
}

std::vector<Song> SessionController::playlistSongs(std::size_t index) const {
    std::vector<Song> songs;
    if (index >= playlists().size()) return songs;
    songs.reserve(playlists()[index].songIds().size());
    for (const Song::Id id : playlists()[index].songIds()) {
        const auto song = library_.findById(id);
        if (song) songs.push_back(song->get());
    }
    return songs;
}

bool SessionController::playLibrary(std::size_t index) {
    if (index >= library_.songs().size()) {
        setMessage("Library selection is out of range.", true);
        return false;
    }
    if (!player_.play(library_.songs(), index)) {
        setMessage(player_.lastError(), true);
        return false;
    }
    setMessage("Now playing: " + player_.currentSong()->title());
    return saveSettings();
}

bool SessionController::playPlaylist(std::size_t playlistIndex, std::size_t trackIndex) {
    auto songs = playlistSongs(playlistIndex);
    if (trackIndex >= songs.size()) {
        setMessage("Playlist track selection is out of range.", true);
        return false;
    }
    if (!player_.play(std::move(songs), trackIndex)) {
        setMessage(player_.lastError(), true);
        return false;
    }
    setActivePlaylist(playlistIndex);
    setMessage("Now playing: " + player_.currentSong()->title());
    return true;
}

bool SessionController::enqueueLibrary(std::size_t index) {
    if (index >= library_.songs().size()) {
        setMessage("Library selection is out of range.", true);
        return false;
    }
    player_.enqueue(library_.songs()[index]);
    setMessage("Added to queue: " + library_.songs()[index].title());
    return true;
}

bool SessionController::enqueuePlaylist(std::size_t index) {
    const auto songs = playlistSongs(index);
    if (songs.empty()) {
        setMessage("The selected playlist has no resolved tracks.", true);
        return false;
    }
    for (const auto& song : songs) player_.enqueue(song);
    setMessage("Added " + std::to_string(songs.size()) + " playlist tracks to the queue.");
    return true;
}

bool SessionController::deletePlaylist(std::size_t index, std::string& error) {
    if (!playlistManager_.erase(index, error)) return false;
    if (activePlaylist_) {
        if (*activePlaylist_ == index) activePlaylist_.reset();
        else if (*activePlaylist_ > index) --*activePlaylist_;
    }
    return saveSettings();
}

bool SessionController::setVolume(float volume) {
    if (!player_.setVolume(volume)) {
        setMessage(player_.lastError(), true);
        return false;
    }
    setMessage("Volume set to " + std::to_string(static_cast<int>(player_.volume() * 100.0F)) + "%.");
    return saveSettings();
}

void SessionController::setMode(PlaybackMode mode) {
    player_.setMode(mode);
    setMessage(std::string("Playback mode: ") + Player::modeName(mode));
    (void)saveSettings();
}

void SessionController::setMessage(std::string message, bool isError) {
    message_ = std::move(message);
    messageIsError_ = isError;
}

bool SessionController::saveSettings() {
    if (!loaded_) return true;
    settings_.playbackMode = Player::modeName(player_.mode());
    settings_.volume = player_.volume();
    if (activePlaylist_ && *activePlaylist_ >= playlists().size()) activePlaylist_.reset();
    settings_.activePlaylist = activePlaylist_ ? playlists()[*activePlaylist_].name() : "";
    if (const Song* song = player_.currentSong()) {
        settings_.lastSong = song->filePath().generic_string();
    }
    std::string error;
    if (!config_.save(settings_, error)) {
        setMessage(error, true);
        return false;
    }
    return true;
}

}  // namespace music_player

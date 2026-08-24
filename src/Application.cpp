#include "Application.h"

#include "DataLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace music_player {
namespace {

const char* stateName(PlaybackState state) {
    switch (state) {
        case PlaybackState::Playing: return "PLAYING";
        case PlaybackState::Paused: return "PAUSED";
        case PlaybackState::Stopped: return "STOPPED";
    }
    return "STOPPED";
}

PlaybackMode nextMode(PlaybackMode mode) {
    switch (mode) {
        case PlaybackMode::NoRepeat: return PlaybackMode::RepeatOne;
        case PlaybackMode::RepeatOne: return PlaybackMode::RepeatAll;
        case PlaybackMode::RepeatAll: return PlaybackMode::Shuffle;
        case PlaybackMode::Shuffle: return PlaybackMode::NoRepeat;
    }
    return PlaybackMode::NoRepeat;
}

}  // namespace

Application::Application(std::istream& input,
                         std::ostream& output,
                         std::filesystem::path dataDirectory)
    : input_(input),
      output_(output),
      dataDirectory_(std::move(dataDirectory)),
      config_(dataDirectory_ / "settings.cfg") {}

int Application::run() {
    loadData();
    renderHeader();
    output_ << "Loaded " << library_.songs().size() << " songs and " << playlists_.size()
            << " playlists from " << dataDirectory_.string() << ".\n";
    for (const auto& warning : loadWarnings_) output_ << "Warning: " << warning << '\n';
    if (!player_.audioAvailable()) output_ << "Warning: " << player_.lastError() << '\n';

    while (currentScreen_ != Screen::Exit) {
        player_.update();
        switch (currentScreen_) {
            case Screen::MainMenu: {
                renderMainMenu();
                const auto command = prompt("> ");
                if (!command) currentScreen_ = Screen::Exit;
                else dispatchMainMenu(normalize(*command));
                break;
            }
            case Screen::Library: runLibrary(); break;
            case Screen::Playlists: runPlaylists(); break;
            case Screen::NowPlaying: runNowPlaying(); break;
            case Screen::Search: runSearch(); break;
            case Screen::Settings: runSettings(); break;
            case Screen::Help: renderHelp(); waitForBack(); break;
            case Screen::Exit: break;
        }
    }

    saveSettings();
    player_.stop();
    output_ << "\nGoodbye. Keep the music playing!\n";
    return 0;
}

void Application::loadData() {
    library_.clear();
    playlists_.clear();
    loadWarnings_.clear();
    auto libraryFile = dataDirectory_ / "library.csv";
    if (!std::filesystem::exists(libraryFile)) libraryFile = dataDirectory_ / "library.csv.example";
    auto report = CsvLoader::load(libraryFile, library_);
    loadWarnings_.insert(loadWarnings_.end(), report.warnings.begin(), report.warnings.end());
    report = M3uLoader::loadDirectory(dataDirectory_ / "Playlists", library_, playlists_);
    loadWarnings_.insert(loadWarnings_.end(), report.warnings.begin(), report.warnings.end());

    settings_ = config_.load();
    player_.setMode(Player::parseMode(settings_.playbackMode));
    player_.setVolume(settings_.volume);
    activePlaylist_.reset();
    for (std::size_t i = 0; i < playlists_.size(); ++i) {
        if (playlists_[i].name() == settings_.activePlaylist) {
            activePlaylist_ = i;
            break;
        }
    }
}

void Application::saveSettings() {
    settings_.playbackMode = Player::modeName(player_.mode());
    settings_.volume = player_.volume();
    settings_.activePlaylist = activePlaylist_ ? playlists_[*activePlaylist_].name() : "";
    if (const Song* song = player_.currentSong()) settings_.lastSong = song->filePath().generic_string();
    std::string error;
    if (!config_.save(settings_, error)) output_ << "Warning: " << error << '\n';
}

void Application::renderHeader() const {
    output_ << "========================================\n"
            << "          TERMINAL MUSIC PLAYER         \n"
            << "========================================\n";
}

void Application::renderMainMenu() const {
    output_ << "\nMAIN MENU\n"
            << "  [1] Library       Browse, filter, sort, and play\n"
            << "  [2] Playlists     Browse M3U playlists\n"
            << "  [3] Now Playing   Playback controls and queue\n"
            << "  [4] Search        Find title, artist, or album\n"
            << "  [5] Settings      Playback mode and volume\n"
            << "  [H] Help\n"
            << "  [Q] Quit\n";
}

void Application::renderHelp() const {
    output_ << "\n--- HELP ---\n"
            << "Commands are case-insensitive; press Enter after each command.\n"
            << "Use 'p NUMBER' to play a song from a displayed list.\n"
            << "Library supports: sort FIELD, filter FIELD VALUE, clear, reload.\n"
            << "Now Playing supports: p, s, n, v, +15, -15, m.\n"
            << "Use B (or an empty line) to return and Q to quit from the main menu.\n";
}

void Application::runLibrary() {
    SongView view = allSongs();
    while (currentScreen_ == Screen::Library) {
        output_ << "\n--- LIBRARY ---\n";
        renderSongs(view);
        output_ << "Commands: p NUMBER | sort title/artist/album/year/duration\n"
                << "          filter artist/album/genre VALUE | clear | reload | b\n";
        const auto raw = prompt("library> ");
        if (!raw) { currentScreen_ = Screen::Exit; return; }
        const auto command = normalize(*raw);
        if (command.empty() || command == "b" || command == "back") {
            currentScreen_ = Screen::MainMenu;
        } else if (command.rfind("p ", 0) == 0) {
            playSelection(view, trim(raw->substr(raw->find_first_of(" \t") + 1)));
        } else if (command == "clear") {
            view = allSongs();
        } else if (command == "reload") {
            player_.stop();
            loadData();
            view = allSongs();
            output_ << "Library reloaded.\n";
        } else if (command.rfind("sort ", 0) == 0) {
            const auto field = trim(command.substr(5));
            if (field == "title") library_.sortByTitle();
            else if (field == "artist") library_.sortByArtist();
            else if (field == "album") library_.sortByAlbum();
            else if (field == "year") library_.sortByYear();
            else if (field == "duration") library_.sortByDuration();
            else { output_ << "Unknown sort field.\n"; continue; }
            view = allSongs();
        } else if (command.rfind("filter ", 0) == 0) {
            std::istringstream parser(raw->substr(7));
            std::string field;
            parser >> field;
            std::string value;
            std::getline(parser, value);
            field = normalize(field);
            value = trim(value);
            if (value.empty()) output_ << "Enter a filter value.\n";
            else if (field == "artist") view = library_.filterByArtist(value);
            else if (field == "album") view = library_.filterByAlbum(value);
            else if (field == "genre") view = library_.filterByGenre(value);
            else output_ << "Filter by artist, album, or genre.\n";
        } else {
            output_ << "Unknown library command.\n";
        }
    }
}

void Application::runPlaylists() {
    while (currentScreen_ == Screen::Playlists) {
        output_ << "\n--- PLAYLISTS ---\n";
        if (playlists_.empty()) output_ << "No .m3u playlists found.\n";
        for (std::size_t i = 0; i < playlists_.size(); ++i) {
            output_ << "  " << (i + 1) << ". " << playlists_[i].name() << " ("
                    << playlists_[i].songIds().size() << " tracks)"
                    << (activePlaylist_ && *activePlaylist_ == i ? " [active]" : "") << '\n';
        }
        output_ << "Enter a playlist number, or B to return.\n";
        const auto raw = prompt("playlists> ");
        if (!raw) { currentScreen_ = Screen::Exit; return; }
        const auto command = normalize(*raw);
        if (command.empty() || command == "b" || command == "back") {
            currentScreen_ = Screen::MainMenu;
            return;
        }
        const auto selection = parseSelection(command, playlists_.size());
        if (!selection) { output_ << "Choose a valid playlist number.\n"; continue; }
        activePlaylist_ = *selection;
        saveSettings();
        const auto songs = playlistSongs(playlists_[*selection]);
        output_ << "\n--- " << playlists_[*selection].name() << " ---\n";
        renderSongs(songs);
        const auto action = prompt("p NUMBER to play, Enter to return> ");
        if (!action) { currentScreen_ = Screen::Exit; return; }
        const auto normalized = normalize(*action);
        if (normalized.rfind("p ", 0) == 0) {
            playSelection(songs, trim(action->substr(action->find_first_of(" \t") + 1)));
        }
    }
}

void Application::runNowPlaying() {
    while (currentScreen_ == Screen::NowPlaying) {
        player_.update();
        output_ << "\n--- NOW PLAYING ---\n";
        const Song* song = player_.currentSong();
        if (!song) {
            output_ << "No song selected. Play one from Library or Playlists.\n";
        } else {
            output_ << song->title() << " - " << song->artist() << '\n'
                    << "State: " << stateName(player_.state())
                    << "  Time: " << formatDuration(player_.positionSeconds()) << " / "
                    << formatDuration(player_.durationSeconds())
                    << "  Mode: " << Player::modeName(player_.mode()) << '\n'
                    << "Queue: " << (player_.currentIndex() + 1) << '/' << player_.queue().size() << '\n';
        }
        output_ << "[P] pause/resume  [S] stop  [N] next  [V] previous\n"
                << "[+15/-15] seek    [M] mode  [B] back\n";
        const auto raw = prompt("player> ");
        if (!raw) { currentScreen_ = Screen::Exit; return; }
        const auto command = normalize(*raw);
        if (command.empty() || command == "b" || command == "back") currentScreen_ = Screen::MainMenu;
        else if (command == "p" || command == "pause" || command == "resume") {
            if (!player_.togglePause()) output_ << player_.lastError() << '\n';
        } else if (command == "s" || command == "stop") player_.stop();
        else if (command == "n" || command == "next") {
            if (!player_.next() && !player_.lastError().empty()) output_ << player_.lastError() << '\n';
        } else if (command == "v" || command == "previous" || command == "prev") {
            if (!player_.previous() && !player_.lastError().empty()) output_ << player_.lastError() << '\n';
        } else if (command == "+15" || command == "forward") player_.seekBy(15.0);
        else if (command == "-15" || command == "rewind") player_.seekBy(-15.0);
        else if (command == "m" || command == "mode") {
            player_.setMode(nextMode(player_.mode()));
            saveSettings();
        } else output_ << "Unknown playback command.\n";
    }
}

void Application::runSearch() {
    output_ << "\n--- SEARCH ---\n";
    const auto query = prompt("Title, artist, or album (Enter to cancel)> ");
    if (!query) { currentScreen_ = Screen::Exit; return; }
    if (!trim(*query).empty()) {
        const auto results = library_.search(trim(*query));
        renderSongs(results);
        const auto action = prompt("p NUMBER to play, Enter to return> ");
        if (!action) { currentScreen_ = Screen::Exit; return; }
        if (normalize(*action).rfind("p ", 0) == 0) {
            playSelection(results, trim(action->substr(action->find_first_of(" \t") + 1)));
        }
    }
    currentScreen_ = Screen::MainMenu;
}

void Application::runSettings() {
    while (currentScreen_ == Screen::Settings) {
        output_ << "\n--- SETTINGS ---\n"
                << "Volume: " << static_cast<int>(std::lround(player_.volume() * 100.0F)) << "%\n"
                << "Playback mode: " << Player::modeName(player_.mode()) << "\n"
                << "Commands: volume 0-100 | mode no/repeat-one/repeat-all/shuffle | b\n";
        const auto raw = prompt("settings> ");
        if (!raw) { currentScreen_ = Screen::Exit; return; }
        const auto command = normalize(*raw);
        if (command.empty() || command == "b" || command == "back") {
            saveSettings();
            currentScreen_ = Screen::MainMenu;
        } else if (command.rfind("volume ", 0) == 0) {
            try {
                std::size_t consumed = 0;
                const auto valueText = trim(command.substr(7));
                const int value = std::stoi(valueText, &consumed);
                if (consumed != valueText.size() || value < 0 || value > 100) throw std::out_of_range("volume");
                player_.setVolume(static_cast<float>(value) / 100.0F);
                saveSettings();
            } catch (...) { output_ << "Volume must be an integer from 0 to 100.\n"; }
        } else if (command.rfind("mode ", 0) == 0) {
            const auto value = trim(command.substr(5));
            if (value == "no" || value == "no-repeat") player_.setMode(PlaybackMode::NoRepeat);
            else if (value == "repeat-one" || value == "one") player_.setMode(PlaybackMode::RepeatOne);
            else if (value == "repeat-all" || value == "all") player_.setMode(PlaybackMode::RepeatAll);
            else if (value == "shuffle") player_.setMode(PlaybackMode::Shuffle);
            else { output_ << "Unknown mode.\n"; continue; }
            saveSettings();
        } else output_ << "Unknown settings command.\n";
    }
}

void Application::dispatchMainMenu(const std::string& command) {
    if (command == "1" || command == "library") currentScreen_ = Screen::Library;
    else if (command == "2" || command == "playlists") currentScreen_ = Screen::Playlists;
    else if (command == "3" || command == "now playing") currentScreen_ = Screen::NowPlaying;
    else if (command == "4" || command == "search") currentScreen_ = Screen::Search;
    else if (command == "5" || command == "settings") currentScreen_ = Screen::Settings;
    else if (command == "h" || command == "help" || command == "?") currentScreen_ = Screen::Help;
    else if (command == "q" || command == "quit" || command == "exit") currentScreen_ = Screen::Exit;
    else output_ << "Invalid command. Enter 1-5, H, or Q.\n";
}

void Application::waitForBack() {
    const auto command = prompt("[B] Back > ");
    if (!command) currentScreen_ = Screen::Exit;
    else currentScreen_ = Screen::MainMenu;
}

Application::SongView Application::allSongs() const {
    SongView result;
    result.reserve(library_.songs().size());
    for (const auto& song : library_.songs()) result.emplace_back(song);
    return result;
}

Application::SongView Application::playlistSongs(const Playlist& playlist) const {
    SongView result;
    for (const auto id : playlist.songIds()) {
        const auto song = library_.findById(id);
        if (song) result.emplace_back(song->get());
    }
    return result;
}

void Application::renderSongs(const SongView& songs) const {
    if (songs.empty()) { output_ << "No songs found.\n"; return; }
    output_ << " #  " << std::left << std::setw(25) << "TITLE" << std::setw(19) << "ARTIST"
            << std::setw(19) << "ALBUM" << std::setw(7) << "YEAR" << "TIME\n";
    output_ << std::string(82, '-') << '\n';
    for (std::size_t i = 0; i < songs.size(); ++i) {
        const auto& song = songs[i].get();
        output_ << std::right << std::setw(2) << (i + 1) << "  " << std::left
                << std::setw(25) << fit(song.title(), 24)
                << std::setw(19) << fit(song.artist(), 18)
                << std::setw(19) << fit(song.album(), 18)
                << std::setw(7) << (song.year() == 0 ? "-" : std::to_string(song.year()))
                << formatDuration(static_cast<double>(song.duration().count()) / 1000.0) << '\n';
    }
    output_ << std::right;
}

bool Application::playSelection(const SongView& songs, const std::string& argument) {
    const auto selection = parseSelection(argument, songs.size());
    if (!selection) { output_ << "Choose a valid song number.\n"; return false; }
    std::vector<Song> queue;
    queue.reserve(songs.size());
    for (const auto song : songs) queue.push_back(song.get());
    if (!player_.play(std::move(queue), *selection)) {
        output_ << "Playback error: " << player_.lastError() << '\n';
        return false;
    }
    saveSettings();
    output_ << "Now playing: " << player_.currentSong()->title() << '\n';
    return true;
}

std::optional<std::string> Application::prompt(const std::string& label) {
    output_ << label;
    std::string value;
    if (!std::getline(input_, value)) return std::nullopt;
    return value;
}

std::string Application::normalize(std::string value) {
    value = trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string Application::trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Application::formatDuration(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    const auto total = static_cast<long long>(seconds);
    std::ostringstream output;
    output << (total / 60) << ':' << std::setfill('0') << std::setw(2) << (total % 60);
    return output.str();
}

std::string Application::fit(std::string value, std::size_t width) {
    if (value.size() <= width) return value;
    if (width < 4) return value.substr(0, width);
    return value.substr(0, width - 3) + "...";
}

std::optional<std::size_t> Application::parseSelection(const std::string& value, std::size_t count) {
    try {
        const auto cleaned = trim(value);
        std::size_t consumed = 0;
        const auto number = std::stoull(cleaned, &consumed);
        if (consumed != cleaned.size() || number == 0 || number > count) return std::nullopt;
        return static_cast<std::size_t>(number - 1);
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace music_player

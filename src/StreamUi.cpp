#include "StreamUi.h"

#include "Utf8.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

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

std::string duration(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    const auto whole = static_cast<long long>(seconds);
    std::ostringstream result;
    result << whole / 60 << ':' << std::setfill('0') << std::setw(2) << whole % 60;
    return result.str();
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

StreamUi::StreamUi(SessionController& session, std::istream& input, std::ostream& output)
    : session_(session), input_(input), output_(output) {}

int StreamUi::run() {
    output_ << "TERMINAL MUSIC PLAYER -- non-interactive mode\n"
            << "Data directory: " << session_.dataDirectory().string() << '\n'
            << session_.message() << '\n';
    for (const auto& warning : session_.warnings()) output_ << "Warning: " << warning << '\n';
    if (!session_.player().audioAvailable()) {
        output_ << "Warning: " << session_.player().lastError() << '\n';
    }
    output_ << "Type 'help' for commands.\n";

    std::string line;
    bool firstLine = true;
    while (std::getline(input_, line)) {
        if (firstLine && line.size() >= 3
            && static_cast<unsigned char>(line[0]) == 0xEF
            && static_cast<unsigned char>(line[1]) == 0xBB
            && static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        firstLine = false;
        (void)session_.tick();
        if (!execute(line)) break;
    }
    return 0;
}

bool StreamUi::execute(const std::string& line) {
    const auto command = lower(trim(line));
    if (command.empty()) return true;
    if (command == "quit" || command == "q" || command == "exit") return false;
    if (command == "help" || command == "?") { help(); return true; }
    if (command == "list" || command == "library") { listLibrary(); return true; }
    if (command == "status") { status(); return true; }
    if (command == "pause" || command == "resume" || command == "toggle") {
        showResult(session_.player().togglePause()); return true;
    }
    if (command == "stop") { showResult(session_.player().stop()); return true; }
    if (command == "next") { showResult(session_.player().next()); return true; }
    if (command == "previous" || command == "prev") {
        showResult(session_.player().previous()); return true;
    }
    if (command == "mode") {
        session_.setMode(nextMode(session_.player().mode()));
        showResult(true);
        return true;
    }
    if (command == "reload") { showResult(session_.reload()); return true; }

    const auto space = command.find(' ');
    const auto verb = space == std::string::npos ? command : command.substr(0, space);
    const auto arguments = space == std::string::npos ? std::string{} : trim(line.substr(space + 1));
    if (verb == "play") {
        const auto selected = index(arguments, session_.library().songs().size());
        showResult(selected && session_.playLibrary(*selected));
    } else if (verb == "seek") {
        try {
            std::size_t consumed = 0;
            const double amount = std::stod(arguments, &consumed);
            showResult(consumed == arguments.size() && session_.player().seekBy(amount));
        } catch (...) { output_ << "Error: seek expects seconds, such as 'seek 15' or 'seek -15'.\n"; }
    } else if (verb == "volume") {
        try {
            std::size_t consumed = 0;
            const int amount = std::stoi(arguments, &consumed);
            showResult(consumed == arguments.size() && amount >= 0 && amount <= 100
                       && session_.setVolume(static_cast<float>(amount) / 100.0F));
        } catch (...) { output_ << "Error: volume expects an integer from 0 to 100.\n"; }
    } else if (verb == "mode") {
        const auto value = lower(arguments);
        if (value == "no" || value == "no-repeat") session_.setMode(PlaybackMode::NoRepeat);
        else if (value == "repeat-one" || value == "one") session_.setMode(PlaybackMode::RepeatOne);
        else if (value == "repeat-all" || value == "all") session_.setMode(PlaybackMode::RepeatAll);
        else if (value == "shuffle") session_.setMode(PlaybackMode::Shuffle);
        else { output_ << "Error: mode expects no-repeat, repeat-one, repeat-all, or shuffle.\n"; return true; }
        showResult(true);
    } else if (verb == "search") {
        const auto results = session_.library().search(arguments);
        if (results.empty()) output_ << "No songs matched.\n";
        for (std::size_t i = 0; i < results.size(); ++i) {
            output_ << i + 1 << ". " << results[i].get().title() << " -- "
                    << results[i].get().artist() << '\n';
        }
    } else if (verb == "queue") {
        std::istringstream parser(arguments);
        std::string action;
        parser >> action;
        std::string rest;
        std::getline(parser, rest);
        rest = trim(rest);
        if (action.empty() || lower(action) == "list") listQueue();
        else if (lower(action) == "clear") { session_.player().clearQueue(); output_ << "OK: queue cleared.\n"; }
        else if (lower(action) == "add") {
            const auto selected = index(rest, session_.library().songs().size());
            showResult(selected && session_.enqueueLibrary(*selected));
        } else if (lower(action) == "remove" || lower(action) == "play") {
            const auto selected = index(rest, session_.player().queue().size());
            const bool success = selected && (lower(action) == "remove"
                ? session_.player().removeFromQueue(*selected) : session_.player().playAt(*selected));
            showResult(success);
        } else if (lower(action) == "move") {
            std::istringstream numbers(rest);
            std::string fromText, toText;
            numbers >> fromText >> toText;
            const auto from = index(fromText, session_.player().queue().size());
            const auto to = index(toText, session_.player().queue().size());
            showResult(from && to && session_.player().moveInQueue(*from, *to));
        } else output_ << "Error: queue expects list/add/remove/move/play/clear.\n";
    } else if (verb == "playlist" || verb == "playlists") {
        std::istringstream parser(arguments);
        std::string action;
        parser >> action;
        std::string rest;
        std::getline(parser, rest);
        rest = trim(rest);
        action = lower(action);
        std::string error;
        if (action.empty() || action == "list") listPlaylists();
        else if (action == "create") showResult(session_.playlistManager().create(rest, error));
        else if (action == "reload") {
            const auto report = session_.playlistManager().reload();
            for (const auto& warning : report.warnings) output_ << "Warning: " << warning << '\n';
            output_ << "OK: playlists reloaded.\n";
        } else if (action == "rename") {
            const auto separator = rest.find(' ');
            const auto selected = index(rest.substr(0, separator), session_.playlists().size());
            const auto name = separator == std::string::npos ? std::string{} : trim(rest.substr(separator + 1));
            showResult(selected && session_.playlistManager().rename(*selected, name, error));
        } else if (action == "delete") {
            std::istringstream values(rest);
            std::string selectedText, confirmation;
            values >> selectedText >> confirmation;
            const auto selected = index(selectedText, session_.playlists().size());
            if (lower(confirmation) != "yes") output_ << "Error: append 'yes' to confirm playlist deletion.\n";
            else showResult(selected && session_.deletePlaylist(*selected, error));
        } else if (action == "enqueue") {
            const auto selected = index(rest, session_.playlists().size());
            showResult(selected && session_.enqueuePlaylist(*selected));
        } else if (action == "add" || action == "remove" || action == "play") {
            std::istringstream values(rest);
            std::string playlistText, trackText;
            values >> playlistText >> trackText;
            const auto playlist = index(playlistText, session_.playlists().size());
            bool success = false;
            if (playlist) {
                const std::size_t count = action == "add" ? session_.library().songs().size()
                    : session_.playlists()[*playlist].songIds().size();
                const auto track = index(trackText, count);
                if (track && action == "add") {
                    success = session_.playlistManager().addTrack(
                        *playlist, session_.library().songs()[*track].id(), error);
                } else if (track && action == "remove") {
                    success = session_.playlistManager().removeTrack(*playlist, *track, error);
                } else if (track) {
                    success = session_.playPlaylist(*playlist, *track);
                }
            }
            showResult(success);
        } else if (action == "move") {
            std::istringstream values(rest);
            std::string playlistText, fromText, toText;
            values >> playlistText >> fromText >> toText;
            const auto playlist = index(playlistText, session_.playlists().size());
            bool success = false;
            if (playlist) {
                const auto count = session_.playlists()[*playlist].songIds().size();
                const auto from = index(fromText, count);
                const auto to = index(toText, count);
                success = from && to && session_.playlistManager().moveTrack(*playlist, *from, *to, error);
            }
            showResult(success);
        } else output_ << "Error: playlist expects list/create/rename/delete/add/remove/move/play/enqueue/reload.\n";
        if (!error.empty()) output_ << "Error: " << error << '\n';
    } else {
        output_ << "Error: unknown command. Type 'help'.\n";
    }
    return true;
}

void StreamUi::help() const {
    output_ << "Commands:\n"
            << "  list | search QUERY | play N | status | pause | stop | next | previous\n"
            << "  seek SECONDS | volume 0-100 | mode [no-repeat|repeat-one|repeat-all|shuffle]\n"
            << "  queue list|add N|remove N|move FROM TO|play N|clear\n"
            << "  playlist list|create NAME|rename N NAME|delete N yes|add P S\n"
            << "           remove P T|move P FROM TO|play P T|enqueue P|reload\n"
            << "  reload | quit\n";
}

void StreamUi::listLibrary() const {
    if (session_.library().songs().empty()) { output_ << "Library is empty.\n"; return; }
    output_ << "#   TITLE                         ARTIST                    TIME\n";
    for (std::size_t i = 0; i < session_.library().songs().size(); ++i) {
        const auto& song = session_.library().songs()[i];
        output_ << std::setw(3) << i + 1 << ' ' << utf8::padRight(song.title(), 29)
                << utf8::padRight(song.artist(), 26)
                << duration(static_cast<double>(song.duration().count()) / 1000.0) << '\n';
    }
}

void StreamUi::listQueue() const {
    if (session_.player().queue().empty()) { output_ << "Queue is empty.\n"; return; }
    for (std::size_t i = 0; i < session_.player().queue().size(); ++i) {
        output_ << (i == session_.player().currentIndex() ? "> " : "  ") << i + 1 << ". "
                << session_.player().queue()[i].title() << '\n';
    }
}

void StreamUi::listPlaylists() const {
    if (session_.playlists().empty()) { output_ << "No playlists found.\n"; return; }
    for (std::size_t i = 0; i < session_.playlists().size(); ++i) {
        output_ << i + 1 << ". " << session_.playlists()[i].name() << " ("
                << session_.playlists()[i].songIds().size() << " tracks)"
                << (session_.activePlaylist() && *session_.activePlaylist() == i ? " [active]" : "")
                << '\n';
    }
}

void StreamUi::status() const {
    const auto* song = session_.player().currentSong();
    output_ << "State: " << stateName(session_.player().state())
            << " | Mode: " << Player::modeName(session_.player().mode())
            << " | Volume: " << static_cast<int>(std::lround(session_.player().volume() * 100.0F))
            << "%\n";
    if (song) output_ << "Track: " << song->title() << " -- " << song->artist()
                      << " | " << duration(session_.player().positionSeconds()) << " / "
                      << duration(session_.player().durationSeconds()) << " | Queue "
                      << session_.player().currentIndex() + 1 << '/' << session_.player().queue().size() << '\n';
    else output_ << "No track selected.\n";
}

void StreamUi::showResult(bool success) {
    if (!success) {
        const auto& playerError = session_.player().lastError();
        output_ << "Error: " << (!playerError.empty() ? playerError : "operation failed") << '\n';
    } else if (!session_.message().empty()) {
        output_ << (session_.messageIsError() ? "Error: " : "OK: ") << session_.message() << '\n';
    } else output_ << "OK\n";
}

std::string StreamUi::trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string StreamUi::lower(std::string value) {
    value = trim(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::optional<std::size_t> StreamUi::index(std::string value, std::size_t count) {
    try {
        value = trim(std::move(value));
        std::size_t consumed = 0;
        const auto number = std::stoull(value, &consumed);
        if (consumed != value.size() || number == 0 || number > count) return std::nullopt;
        return static_cast<std::size_t>(number - 1);
    } catch (...) { return std::nullopt; }
}

}  // namespace music_player

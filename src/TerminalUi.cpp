#include "TerminalUi.h"

#include "Utf8.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace music_player {
namespace {

using namespace ftxui;

enum class Screen { Overview, Playlists, Search, Settings };
enum class Focus { Library, Player, Queue, PlaylistList, PlaylistTracks, SearchInput, SearchResults };
enum class EditAction { None, CreatePlaylist, RenamePlaylist };

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
    std::ostringstream output;
    output << whole / 60 << ':' << std::setfill('0') << std::setw(2) << whole % 60;
    return output.str();
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

Element selectedRow(std::string value, bool selected, bool focused) {
    Element row = text(std::move(value));
    if (selected) row = row | (focused ? inverted : bgcolor(Color::GrayDark));
    return row;
}

class UiState {
public:
    explicit UiState(SessionController& controller)
        : session(controller), editor(Input(&editText, "playlist name")),
          searchInput(Input(&searchText, "title, artist, or album")) {}

    Element render() {
        std::vector<Element> layers;
        layers.push_back(vbox({navigation(), separator(), body() | flex,
                               separator(), footer(), messageBar()}));
        if (helpVisible) layers.push_back(helpOverlay());
        if (editAction != EditAction::None) layers.push_back(editOverlay());
        if (deleteConfirmation) layers.push_back(confirmOverlay());
        return dbox(std::move(layers));
    }

    bool event(const Event& event, ScreenInteractive& terminal) {
        if (event == Event::Custom) {
            (void)session.tick();
            clampSelections();
            return true;
        }
        if (deleteConfirmation) return confirmationEvent(event);
        if (editAction != EditAction::None) return editorEvent(event);
        if (helpVisible) {
            if (event == Event::Escape || event == Event::Character("?") || event == Event::F1) {
                helpVisible = false;
                return true;
            }
            return true;
        }
        if (screen == Screen::Search && focus == Focus::SearchInput) {
            if (event == Event::Escape) { screen = Screen::Overview; focus = Focus::Library; return true; }
            if (event == Event::Return || event == Event::ArrowDown || event == Event::Tab) {
                focus = Focus::SearchResults;
                searchSelection = 0;
                return true;
            }
            if (event == Event::F1) { helpVisible = true; return true; }
            return searchInput->OnEvent(event);
        }

        if (event == Event::Character("q")) { terminal.ExitLoopClosure()(); return true; }
        if (event == Event::Character("?") || event == Event::F1) { helpVisible = true; return true; }
        if (event == Event::Character("1")) { screen = Screen::Overview; focus = Focus::Library; return true; }
        if (event == Event::Character("2")) { screen = Screen::Playlists; focus = Focus::PlaylistList; return true; }
        if (event == Event::Character("3")) { screen = Screen::Overview; focus = Focus::Queue; return true; }
        if (event == Event::Character("4") || event == Event::Character("/")) {
            screen = Screen::Search; focus = Focus::SearchInput; return true;
        }
        if (event == Event::Character("5")) { screen = Screen::Settings; focus = Focus::Player; return true; }
        if (event == Event::Escape) { screen = Screen::Overview; focus = Focus::Library; return true; }
        if (event == Event::Character(" ")) { result(session.player().togglePause()); return true; }
        if (event == Event::Character("s")) { result(session.player().stop()); return true; }
        if (event == Event::Character("n")) { result(session.player().next()); return true; }
        if (event == Event::Character("p")) { result(session.player().previous()); return true; }
        if (event == Event::Character("h")) { result(session.player().seekBy(-15.0)); return true; }
        if (event == Event::Character("l")) { result(session.player().seekBy(15.0)); return true; }
        if (event == Event::Character("+") || event == Event::Character("=")) {
            result(session.setVolume(session.player().volume() + 0.05F)); return true;
        }
        if (event == Event::Character("-") || event == Event::Character("_")) {
            result(session.setVolume(session.player().volume() - 0.05F)); return true;
        }
        if (event == Event::Character("m")) { session.setMode(nextMode(session.player().mode())); return true; }
        if (event == Event::Character("o")) { result(session.reload()); return true; }

        if (event == Event::Tab) { cycleFocus(); return true; }
        if (event == Event::ArrowUp) { moveSelection(-1); return true; }
        if (event == Event::ArrowDown) { moveSelection(1); return true; }
        if (event == Event::ArrowLeft || event == Event::ArrowRight) { horizontal(event == Event::ArrowRight); return true; }
        if (event == Event::Return) { activate(); return true; }
        if (event == Event::Character("a")) { add(); return true; }
        if (event == Event::Character("e")) { enqueuePlaylist(); return true; }
        if (event == Event::Character("c")) { createOrClear(); return true; }
        if (event == Event::Character("r")) { beginRename(); return true; }
        if (event == Event::Character("x") || event == Event::Delete) { remove(); return true; }
        if (event == Event::Character("u")) { reorder(-1); return true; }
        if (event == Event::Character("d")) { reorder(1); return true; }
        return false;
    }

private:
    Element navigation() const {
        const auto item = [this](const char* number, const char* name, Screen target) {
            Element value = text(std::string("[") + number + "] " + name);
            return screen == target ? value | bold | color(Color::Cyan) : value | dim;
        };
        return hbox({text(" TERMINAL MUSIC PLAYER ") | bold | color(Color::Cyan), filler(),
                     item("1", "Library", Screen::Overview), text("  "),
                     item("2", "Playlists", Screen::Playlists), text("  "),
                     text("[3] Queue") | (focus == Focus::Queue ? bold : dim), text("  "),
                     item("4", "Search", Screen::Search), text("  "),
                     item("5", "Settings", Screen::Settings), text(" ")});
    }

    Element body() {
        if (screen == Screen::Playlists) return playlistsScreen();
        if (screen == Screen::Search) return searchScreen();
        if (screen == Screen::Settings) return settingsScreen();
        return overviewScreen();
    }

    Element libraryPanel() const {
        std::vector<Element> rows;
        if (session.library().songs().empty()) {
            rows.push_back(text("No songs loaded.") | dim);
            rows.push_back(text("Add library.csv and press O to reload.") | dim);
        }
        for (std::size_t index = 0; index < session.library().songs().size(); ++index) {
            const auto& song = session.library().songs()[index];
            rows.push_back(selectedRow(utf8::truncate(song.title(), 26) + "  "
                                           + duration(song.duration().count() / 1000.0),
                                       index == librarySelection, focus == Focus::Library));
            rows.push_back(text("  " + utf8::truncate(song.artist(), 30)) | dim);
        }
        return window(text(" LIBRARY ") | bold | color(Color::Green),
                      vbox(std::move(rows)) | vscroll_indicator | frame | flex)
            | (focus == Focus::Library ? borderStyled(ROUNDED, Color::Green) : border);
    }

    Element playerPanel() const {
        const auto& player = session.player();
        const Song* song = player.currentSong();
        if (!song) {
            return window(text(" NOW PLAYING ") | bold | color(Color::Cyan),
                          vbox({filler(), text("No track selected") | center | dim,
                                text("Press Enter on a library track") | center | dim, filler()}))
                | (focus == Focus::Player ? borderStyled(ROUNDED, Color::Cyan) : border);
        }
        const double total = player.durationSeconds();
        const double position = player.positionSeconds();
        const float progress = total > 0.0
            ? static_cast<float>(std::clamp(position / total, 0.0, 1.0)) : 0.0F;
        const auto stateColor = player.state() == PlaybackState::Playing ? Color::Green
            : player.state() == PlaybackState::Paused ? Color::Yellow : Color::GrayDark;
        return window(text(" NOW PLAYING ") | bold | color(Color::Cyan),
                      vbox({filler(), text(utf8::truncate(song->title(), 42)) | bold | center,
                            text(utf8::truncate(song->artist() + " -- " + song->album(), 50)) | center | dim,
                            separatorEmpty(), gauge(progress) | color(Color::Cyan),
                            hbox({text(duration(position)), filler(), text(duration(total))}),
                            separatorEmpty(),
                            hbox({text(stateName(player.state())) | bold | color(stateColor), filler(),
                                  text(Player::modeName(player.mode())) | color(Color::Magenta)}),
                            hbox({text("Volume "), gauge(player.volume()) | flex,
                                  text(" " + std::to_string(static_cast<int>(std::lround(player.volume() * 100))) + "%")}),
                            filler()}))
            | (focus == Focus::Player ? borderStyled(ROUNDED, Color::Cyan) : border);
    }

    Element queuePanel() const {
        std::vector<Element> rows;
        if (session.player().queue().empty()) rows.push_back(text("Queue is empty.") | dim);
        for (std::size_t index = 0; index < session.player().queue().size(); ++index) {
            const auto& song = session.player().queue()[index];
            const std::string prefix = index == session.player().currentIndex() ? "> " : "  ";
            rows.push_back(selectedRow(prefix + std::to_string(index + 1) + ". "
                                           + utf8::truncate(song.title(), 25),
                                       index == queueSelection, focus == Focus::Queue));
        }
        return window(text(" QUEUE ") | bold | color(Color::Magenta),
                      vbox(std::move(rows)) | vscroll_indicator | frame | flex)
            | (focus == Focus::Queue ? borderStyled(ROUNDED, Color::Magenta) : border);
    }

    Element overviewScreen() const {
        return hbox({libraryPanel() | flex, playerPanel() | flex, queuePanel() | flex});
    }

    Element playlistsScreen() const {
        std::vector<Element> playlistRows;
        if (session.playlists().empty()) playlistRows.push_back(text("No playlists. Press C to create one.") | dim);
        for (std::size_t index = 0; index < session.playlists().size(); ++index) {
            const auto& playlist = session.playlists()[index];
            const bool active = session.activePlaylist() && *session.activePlaylist() == index;
            playlistRows.push_back(selectedRow(
                std::to_string(index + 1) + ". " + utf8::truncate(playlist.name(), 28)
                    + " (" + std::to_string(playlist.songIds().size()) + ")"
                    + (active ? " [active]" : ""),
                index == playlistSelection, focus == Focus::PlaylistList));
        }

        std::vector<Element> trackRows;
        if (playlistSelection < session.playlists().size()) {
            const auto songs = session.playlistSongs(playlistSelection);
            if (songs.empty()) trackRows.push_back(text("This playlist is empty.") | dim);
            for (std::size_t index = 0; index < songs.size(); ++index) {
                trackRows.push_back(selectedRow(std::to_string(index + 1) + ". "
                                                    + utf8::truncate(songs[index].title(), 38),
                                                index == playlistTrackSelection,
                                                focus == Focus::PlaylistTracks));
            }
        }
        return hbox({
            window(text(" PLAYLISTS ") | bold | color(Color::Green),
                   vbox(std::move(playlistRows)) | vscroll_indicator | frame | flex)
                | (focus == Focus::PlaylistList ? borderStyled(ROUNDED, Color::Green) : border) | flex,
            window(text(" TRACKS ") | bold | color(Color::Cyan),
                   vbox(std::move(trackRows)) | vscroll_indicator | frame | flex)
                | (focus == Focus::PlaylistTracks ? borderStyled(ROUNDED, Color::Cyan) : border) | flex});
    }

    Element searchScreen() {
        const auto results = session.library().search(searchText);
        std::vector<Element> rows;
        if (!searchText.empty() && results.empty()) rows.push_back(text("No songs matched.") | dim);
        for (std::size_t index = 0; index < results.size(); ++index) {
            const auto& song = results[index].get();
            rows.push_back(selectedRow(std::to_string(index + 1) + ". "
                                           + utf8::truncate(song.title(), 35) + " -- "
                                           + utf8::truncate(song.artist(), 28),
                                       index == searchSelection, focus == Focus::SearchResults));
        }
        return window(text(" SEARCH ") | bold | color(Color::Cyan),
                      vbox({hbox({text("Query: "), searchInput->Render() | flex})
                                | (focus == Focus::SearchInput ? borderStyled(ROUNDED, Color::Cyan) : border),
                            separator(), vbox(std::move(rows)) | vscroll_indicator | frame | flex}));
    }

    Element settingsScreen() const {
        return window(text(" SETTINGS ") | bold | color(Color::Magenta),
                      vbox({filler(), text("Volume") | bold | center,
                            hbox({text("0% "), gauge(session.player().volume()) | flex, text(" 100%")}),
                            text(std::to_string(static_cast<int>(std::lround(session.player().volume() * 100))) + "%") | center,
                            separatorEmpty(), text("Playback mode") | bold | center,
                            text(Player::modeName(session.player().mode())) | color(Color::Yellow) | center,
                            separatorEmpty(), text("Left/Right: volume   M: change mode") | center | dim,
                            filler()})) | borderStyled(ROUNDED, Color::Magenta);
    }

    Element footer() const {
        std::string shortcuts = "[Space] Play/Pause  [S] Stop  [N/P] Next/Previous  [H/L] Seek  [-/+] Volume  [M] Mode";
        if (screen == Screen::Overview && focus == Focus::Library) shortcuts += "  [A] Enqueue";
        if (screen == Screen::Overview && focus == Focus::Queue) shortcuts += "  [X] Remove  [U/D] Reorder  [C] Clear";
        if (screen == Screen::Playlists) shortcuts += "  [C] Create  [R] Rename  [X] Delete/Remove  [A] Add  [E] Enqueue";
        return text(shortcuts) | center | dim;
    }

    Element messageBar() const {
        Element value = text(" " + session.message());
        if (session.messageIsError()) value = value | color(Color::Red) | bold;
        else value = value | color(Color::Green);
        return hbox({value, filler(), text("[?] Help  [Q] Quit ") | dim});
    }

    Element helpOverlay() const {
        return window(text(" KEYBOARD SHORTCUTS ") | bold | color(Color::Yellow),
                      vbox({text("1 Library  2 Playlists  3 Queue  4 Search  5 Settings"),
                            text("Arrow keys / J K: move selection   Tab: change focus   Enter: activate"),
                            text("Space play/pause   S stop   N next   P previous   H/L seek 15 seconds"),
                            text("-/+ volume   M playback mode   A add   X remove   U/D reorder"),
                            text("C create/clear   R rename   E enqueue playlist   O reload"),
                            separator(), text("Press ? or Escape to close") | center | dim}))
            | clear_under | center | size(WIDTH, GREATER_THAN, 74);
    }

    Element editOverlay() {
        const char* title = editAction == EditAction::CreatePlaylist ? " CREATE PLAYLIST " : " RENAME PLAYLIST ";
        return window(text(title) | bold | color(Color::Yellow),
                      vbox({editor->Render() | border, text("Enter: save   Escape: cancel") | dim}))
            | clear_under | center | size(WIDTH, EQUAL, 52);
    }

    Element confirmOverlay() const {
        const std::string name = playlistSelection < session.playlists().size()
            ? session.playlists()[playlistSelection].name() : "selected playlist";
        return window(text(" DELETE PLAYLIST ") | bold | color(Color::Red),
                      vbox({text("Delete '" + name + "' from disk?") | center,
                            text("Press Y to delete or N/Escape to cancel") | center | dim}))
            | clear_under | center | size(WIDTH, EQUAL, 58);
    }

    bool editorEvent(const Event& event) {
        if (event == Event::Escape) { editAction = EditAction::None; editText.clear(); return true; }
        if (event != Event::Return) return editor->OnEvent(event);
        std::string error;
        bool success = false;
        if (editAction == EditAction::CreatePlaylist) {
            success = session.playlistManager().create(editText, error);
            if (success) playlistSelection = session.playlists().size() - 1;
        } else {
            success = session.playlistManager().rename(playlistSelection, editText, error);
        }
        session.setMessage(success ? "Playlist saved." : error, !success);
        if (success) { editAction = EditAction::None; editText.clear(); }
        return true;
    }

    bool confirmationEvent(const Event& event) {
        if (event == Event::Character("y") || event == Event::Character("Y")) {
            std::string error;
            const bool success = session.deletePlaylist(playlistSelection, error);
            session.setMessage(success ? "Playlist deleted." : error, !success);
            deleteConfirmation = false;
            clampSelections();
            return true;
        }
        if (event == Event::Character("n") || event == Event::Character("N") || event == Event::Escape) {
            deleteConfirmation = false;
            session.setMessage("Playlist deletion cancelled.");
            return true;
        }
        return true;
    }

    void result(bool success) {
        if (!success) session.setMessage(session.player().lastError().empty()
                                             ? "Operation failed." : session.player().lastError(), true);
    }

    void cycleFocus() {
        if (screen == Screen::Overview) {
            focus = focus == Focus::Library ? Focus::Player
                : focus == Focus::Player ? Focus::Queue : Focus::Library;
        } else if (screen == Screen::Playlists) {
            focus = focus == Focus::PlaylistList ? Focus::PlaylistTracks : Focus::PlaylistList;
        } else if (screen == Screen::Search) {
            focus = focus == Focus::SearchInput ? Focus::SearchResults : Focus::SearchInput;
        }
    }

    void moveSelection(int delta) {
        auto move = [delta](std::size_t& selected, std::size_t count) {
            if (count == 0) { selected = 0; return; }
            if (delta < 0) selected = selected == 0 ? count - 1 : selected - 1;
            else selected = (selected + 1) % count;
        };
        if (focus == Focus::Library) move(librarySelection, session.library().songs().size());
        else if (focus == Focus::Queue) move(queueSelection, session.player().queue().size());
        else if (focus == Focus::PlaylistList) {
            move(playlistSelection, session.playlists().size());
            playlistTrackSelection = 0;
        } else if (focus == Focus::PlaylistTracks) {
            const auto count = playlistSelection < session.playlists().size()
                ? session.playlists()[playlistSelection].songIds().size() : 0;
            move(playlistTrackSelection, count);
        } else if (focus == Focus::SearchResults) move(searchSelection, session.library().search(searchText).size());
    }

    void horizontal(bool right) {
        if (screen == Screen::Settings || focus == Focus::Player) {
            result(session.setVolume(session.player().volume() + (right ? 0.05F : -0.05F)));
        } else cycleFocus();
    }

    void activate() {
        if (focus == Focus::Library) result(session.playLibrary(librarySelection));
        else if (focus == Focus::Player) result(session.player().togglePause());
        else if (focus == Focus::Queue) result(session.player().playAt(queueSelection));
        else if (focus == Focus::PlaylistList && playlistSelection < session.playlists().size()) {
            session.setActivePlaylist(playlistSelection);
            focus = Focus::PlaylistTracks;
        } else if (focus == Focus::PlaylistTracks) {
            result(session.playPlaylist(playlistSelection, playlistTrackSelection));
        } else if (focus == Focus::SearchResults) {
            const auto results = session.library().search(searchText);
            if (searchSelection < results.size()) {
                std::vector<Song> queue;
                queue.reserve(results.size());
                for (const auto& song : results) queue.push_back(song.get());
                result(session.player().play(std::move(queue), searchSelection));
            }
        }
    }

    void add() {
        if (screen == Screen::Overview && focus == Focus::Library) {
            result(session.enqueueLibrary(librarySelection));
        } else if (screen == Screen::Playlists && playlistSelection < session.playlists().size()
                   && librarySelection < session.library().songs().size()) {
            std::string error;
            const bool success = session.playlistManager().addTrack(
                playlistSelection, session.library().songs()[librarySelection].id(), error);
            session.setMessage(success ? "Track added to playlist." : error, !success);
        }
    }

    void enqueuePlaylist() {
        if (screen == Screen::Playlists) result(session.enqueuePlaylist(playlistSelection));
    }

    void createOrClear() {
        if (screen == Screen::Playlists) {
            editAction = EditAction::CreatePlaylist;
            editText.clear();
        } else if (screen == Screen::Overview && focus == Focus::Queue) {
            session.player().clearQueue();
            session.setMessage("Queue cleared.");
        }
    }

    void beginRename() {
        if (screen != Screen::Playlists || playlistSelection >= session.playlists().size()) return;
        editAction = EditAction::RenamePlaylist;
        editText = session.playlists()[playlistSelection].name();
    }

    void remove() {
        if (screen == Screen::Overview && focus == Focus::Queue) {
            result(session.player().removeFromQueue(queueSelection));
            clampSelections();
        } else if (screen == Screen::Playlists && focus == Focus::PlaylistList
                   && playlistSelection < session.playlists().size()) {
            deleteConfirmation = true;
        } else if (screen == Screen::Playlists && focus == Focus::PlaylistTracks) {
            std::string error;
            const bool success = session.playlistManager().removeTrack(
                playlistSelection, playlistTrackSelection, error);
            session.setMessage(success ? "Track removed from playlist." : error, !success);
            clampSelections();
        }
    }

    void reorder(int delta) {
        if (screen == Screen::Overview && focus == Focus::Queue && !session.player().queue().empty()) {
            const auto target = delta < 0
                ? (queueSelection == 0 ? 0 : queueSelection - 1)
                : std::min(queueSelection + 1, session.player().queue().size() - 1);
            result(session.player().moveInQueue(queueSelection, target));
            queueSelection = target;
        } else if (screen == Screen::Playlists && focus == Focus::PlaylistTracks
                   && playlistSelection < session.playlists().size()
                   && !session.playlists()[playlistSelection].songIds().empty()) {
            const auto count = session.playlists()[playlistSelection].songIds().size();
            const auto target = delta < 0
                ? (playlistTrackSelection == 0 ? 0 : playlistTrackSelection - 1)
                : std::min(playlistTrackSelection + 1, count - 1);
            std::string error;
            const bool success = session.playlistManager().moveTrack(
                playlistSelection, playlistTrackSelection, target, error);
            session.setMessage(success ? "Playlist order saved." : error, !success);
            if (success) playlistTrackSelection = target;
        }
    }

    void clampSelections() {
        const auto clamp = [](std::size_t& selected, std::size_t count) {
            if (count == 0) selected = 0;
            else if (selected >= count) selected = count - 1;
        };
        clamp(librarySelection, session.library().songs().size());
        clamp(queueSelection, session.player().queue().size());
        clamp(playlistSelection, session.playlists().size());
        const auto tracks = playlistSelection < session.playlists().size()
            ? session.playlists()[playlistSelection].songIds().size() : 0;
        clamp(playlistTrackSelection, tracks);
        clamp(searchSelection, session.library().search(searchText).size());
    }

    SessionController& session;
    Screen screen{Screen::Overview};
    Focus focus{Focus::Library};
    EditAction editAction{EditAction::None};
    std::string editText;
    std::string searchText;
    std::size_t librarySelection{0};
    std::size_t queueSelection{0};
    std::size_t playlistSelection{0};
    std::size_t playlistTrackSelection{0};
    std::size_t searchSelection{0};
    bool helpVisible{false};
    bool deleteConfirmation{false};

public:
    Component editor;
    Component searchInput;
};

}  // namespace

TerminalUi::TerminalUi(SessionController& session) : session_(session) {}

int TerminalUi::run() {
    auto terminal = ScreenInteractive::Fullscreen();
    UiState state{session_};
    auto root = Container::Vertical({state.editor, state.searchInput});
    auto renderer = Renderer(root, [&] { return state.render(); });
    auto events = CatchEvent(renderer, [&](const Event& event) { return state.event(event, terminal); });

    std::atomic<bool> running{true};
    std::thread ticker([&] {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (running.load()) terminal.PostEvent(Event::Custom);
        }
    });
    terminal.Loop(events);
    running.store(false);
    ticker.join();
    return 0;
}

std::string TerminalUi::snapshot(int width, int height) {
    UiState state{session_};
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                        ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, state.render());
    return screen.ToString();
}

}  // namespace music_player

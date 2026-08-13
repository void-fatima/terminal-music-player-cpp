#include "Application.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

namespace music_player {

Application::Application(std::istream& input, std::ostream& output)
    : input_(input), output_(output) {}

int Application::run() {
    renderHeader();

    while (currentScreen_ != Screen::Exit) {
        if (currentScreen_ == Screen::MainMenu) {
            renderMainMenu();
            output_ << "> ";

            std::string command;
            if (!std::getline(input_, command)) {
                currentScreen_ = Screen::Exit;
                break;
            }
            dispatchMainMenu(normalize(command));
        } else if (currentScreen_ == Screen::Help) {
            renderHelp();
            waitForBack();
        } else {
            renderPlaceholder(currentScreen_);
            waitForBack();
        }
    }

    output_ << "\nGoodbye. Keep the music playing!\n";
    return 0;
}

void Application::renderHeader() const {
    output_ << "========================================\n"
            << "          TERMINAL MUSIC PLAYER         \n"
            << "========================================\n";
}

void Application::renderMainMenu() const {
    output_ << "\nMAIN MENU\n"
            << "  [1] Library       Browse all songs\n"
            << "  [2] Playlists     Browse and select playlists\n"
            << "  [3] Now Playing   Playback controls and queue\n"
            << "  [4] Search        Find songs, artists, or albums\n"
            << "  [5] Settings      Playback and app preferences\n"
            << "  [H] Help          Show navigation commands\n"
            << "  [Q] Quit\n";
}

void Application::renderPlaceholder(Screen screen) const {
    output_ << "\n--- " << screenTitle(screen) << " ---\n"
            << "This module is ready for its feature implementation.\n";
}

void Application::renderHelp() const {
    output_ << "\n--- HELP ---\n"
            << "Choose an item with its number, then press Enter.\n"
            << "Commands are case-insensitive.\n"
            << "Use B to return from a screen and Q to quit from the main menu.\n";
}

void Application::dispatchMainMenu(const std::string& command) {
    if (command == "1" || command == "library") {
        currentScreen_ = Screen::Library;
    } else if (command == "2" || command == "playlists") {
        currentScreen_ = Screen::Playlists;
    } else if (command == "3" || command == "now playing") {
        currentScreen_ = Screen::NowPlaying;
    } else if (command == "4" || command == "search") {
        currentScreen_ = Screen::Search;
    } else if (command == "5" || command == "settings") {
        currentScreen_ = Screen::Settings;
    } else if (command == "h" || command == "help" || command == "?") {
        currentScreen_ = Screen::Help;
    } else if (command == "q" || command == "quit" || command == "exit") {
        currentScreen_ = Screen::Exit;
    } else {
        output_ << "Invalid command. Enter 1-5, H, or Q.\n";
    }
}

void Application::waitForBack() {
    output_ << "[B] Back > ";
    std::string command;
    while (std::getline(input_, command)) {
        command = normalize(command);
        if (command == "b" || command == "back" || command.empty()) {
            currentScreen_ = Screen::MainMenu;
            return;
        }
        output_ << "Enter B to return to the main menu > ";
    }
    currentScreen_ = Screen::Exit;
}

std::string Application::normalize(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

const char* Application::screenTitle(Screen screen) {
    switch (screen) {
        case Screen::Library: return "LIBRARY";
        case Screen::Playlists: return "PLAYLISTS";
        case Screen::NowPlaying: return "NOW PLAYING";
        case Screen::Search: return "SEARCH";
        case Screen::Settings: return "SETTINGS";
        case Screen::Help: return "HELP";
        case Screen::MainMenu: return "MAIN MENU";
        case Screen::Exit: return "EXIT";
    }
    return "UNKNOWN";
}

}  // namespace music_player

#pragma once

#include <iosfwd>
#include <string>

namespace music_player {

class Application {
public:
    Application(std::istream& input, std::ostream& output);
    int run();

private:
    enum class Screen {
        MainMenu,
        Library,
        Playlists,
        NowPlaying,
        Search,
        Settings,
        Help,
        Exit
    };

    void renderHeader() const;
    void renderMainMenu() const;
    void renderPlaceholder(Screen screen) const;
    void renderHelp() const;
    void dispatchMainMenu(const std::string& command);
    void waitForBack();

    static std::string normalize(std::string value);
    static const char* screenTitle(Screen screen);

    std::istream& input_;
    std::ostream& output_;
    Screen currentScreen_{Screen::MainMenu};
};

}  // namespace music_player

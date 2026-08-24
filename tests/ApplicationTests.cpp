#include "Application.h"
#include "ConfigManager.h"

#include "TestSupport.h"

#include <sstream>
#include <string>

int main() {
    using namespace music_player;
    test_support::Runner test;
    test_support::TempDirectory fixture{"application"};
    const auto data = fixture.path();
    test_support::writeFile(data / "music" / "first.wav", "fixture");
    test_support::writeFile(data / "music" / "second.wav", "fixture");
    test_support::writeFile(data / "library.csv",
        "title,artist,album,genre,year,duration_sec,file_path\n"
        "First Track,Example Artist,Example Album,Rock,2025,90,music/first.wav\n"
        "Second Track,Example Artist,Example Album,Jazz,2024,80,music/second.wav\n");
    std::error_code error;
    std::filesystem::create_directories(data / "Playlists", error);

    std::istringstream input{
        "list\n"
        "queue add 1\n"
        "queue add 2\n"
        "queue move 2 1\n"
        "queue play 1\n"
        "status\n"
        "pause\n"
        "status\n"
        "volume 40\n"
        "mode repeat-all\n"
        "playlist create Favorites\n"
        "playlist add 1 1\n"
        "playlist rename 1 Focus\n"
        "playlist list\n"
        "search First\n"
        "not-a-command\n"
        "quit\n"};
    std::ostringstream output;
    auto lifetime = std::make_shared<test_support::FakeAudioState>();
    Application application{input, output, data,
                            std::make_unique<test_support::FakeAudioBackend>(lifetime)};
    test.expect(application.run() == 0, "stream application exits successfully");
    const auto text = output.str();
    test.expect(text.find("Loaded 2 songs and 0 playlists") != std::string::npos,
                "startup reports loaded content truthfully");
    test.expect(text.find("First Track") != std::string::npos
                    && text.find("Second Track") != std::string::npos,
                "library, queue, and search display real track metadata");
    test.expect(text.find("Queue 1/2") != std::string::npos,
                "status identifies the selected queue item");
    test.expect(text.find("State: PAUSED") != std::string::npos,
                "stream commands drive playback state through the fake backend");
    test.expect(text.find("Focus (1 tracks)") != std::string::npos,
                "playlist create, add, rename, persist, and list workflow is functional");
    test.expect(text.find("unknown command") != std::string::npos,
                "invalid stream input produces an actionable error");
    test.expect(lifetime->loads > 0 && lifetime->destroyed == 1,
                "application tests use and clean up only the injected fake backend");

    const auto settings = ConfigManager(data / "settings.cfg").load();
    test.expect(settings.playbackMode == "REPEAT_ALL", "playback mode persists");
    test.expect(settings.volume > 0.39F && settings.volume < 0.41F, "volume persists");
    test.expect(settings.lastSong.find("second.wav") != std::string::npos,
                "selected queue track persists for session restore");

    std::istringstream eofInput;
    std::ostringstream eofOutput;
    Application eofApplication{eofInput, eofOutput, data,
                               std::make_unique<test_support::FakeAudioBackend>()};
    test.expect(eofApplication.run() == 0,
                "redirected EOF exits cleanly without blocking or requiring a command");
    return test.finish("application");
}

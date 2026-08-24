#include "CliOptions.h"
#include "ConfigManager.h"
#include "DataLoader.h"
#include "PlaylistManager.h"
#include "StableId.h"
#include "Utf8.h"

#include "TestSupport.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using music_player::Song;
using test_support::Runner;

void songAndStableIdTests(Runner& test) {
    const Song song{42, "Title", "Artist", "Album", "Genre", "music/song.mp3",
                    Song::Duration{123000}, 2024};
    test.expect(song.id() == 42 && song.title() == "Title" && song.duration().count() == 123000,
                "Song preserves its validated value fields");
    test.expect(music_player::stableSongId("Music/./TRACK.mp3")
                    == music_player::stableSongId("music/track.mp3"),
                "stable IDs normalize separators, dot segments, and ASCII case");
    test.expect(music_player::stableSongId("music/a.mp3")
                    != music_player::stableSongId("music/b.mp3"),
                "different normalized paths receive different stable IDs");
}

void libraryIndexTests(Runner& test) {
    music_player::MusicLibrary library;
    constexpr std::size_t count = 20000;
    for (std::size_t index = 0; index < count; ++index) {
        const auto path = std::filesystem::path("music") / (std::to_string(index) + ".mp3");
        const auto id = music_player::stableSongId(path);
        test.expect(library.addSong(Song{id, "Track " + std::to_string(count - index), "Artist",
                                         "Album", "Genre", path, Song::Duration{1000}, 2020}),
                    "large synthetic library inserts unique songs");
    }
    test.expect(library.songs().size() == count, "large synthetic library retains every song");
    const auto id = music_player::stableSongId("music/12345.mp3");
    test.expect(library.findById(id) && library.findById(id)->get().filePath().filename() == "12345.mp3",
                "indexed ID lookup finds a large-library entry");
    test.expect(library.findByPath("music/9999.mp3").has_value(),
                "indexed normalized-path lookup finds an entry");
    test.expect(!library.addSong(Song{id, "Duplicate", "", "", "", "other.mp3",
                                      Song::Duration{1000}, 2020}),
                "duplicate IDs are rejected");
    library.sortByTitle();
    test.expect(library.findById(id).has_value(), "indexes remain valid after sorting");
    library.clear();
    test.expect(!library.findById(id) && !library.findByPath("music/9999.mp3"),
                "indexes are cleared with the library");
}

void csvTests(Runner& test) {
    test_support::TempDirectory fixture{"csv"};
    const auto data = fixture.path();
    test_support::writeFile(data / "music" / "a.mp3", "fixture");
    test_support::writeFile(data / "library.csv",
        "\xEF\xBB\xBFtitle,artist,album,genre,year,duration_sec,file_path\n"
        "\"A, \"\"Quoted\"\" Song\",Artist,Album,Rock,2024,125,music/a.mp3\n"
        "\"Bad\"tail,Artist,Album,Rock,2024,20,music/b.mp3\n"
        "Duplicate,Artist,Album,Rock,2024,20,./music/A.mp3\n"
        "Future,Artist,Album,Rock,9999,20,music/future.mp3\n");
    music_player::MusicLibrary library;
    const auto report = music_player::CsvLoader::load(data / "library.csv", library);
    test.expect(report.loaded == 1 && library.songs().size() == 1,
                "CSV loaded count includes only inserted songs");
    test.expect(library.songs().front().title() == "A, \"Quoted\" Song",
                "CSV preserves quoted commas and escaped quotes");
    test.expect(std::any_of(report.warnings.begin(), report.warnings.end(), [](const auto& warning) {
                    return warning.find("line 3") != std::string::npos
                        && warning.find("closing quote") != std::string::npos;
                }), "CSV rejects text after a closing quote with a line number");
    test.expect(std::any_of(report.warnings.begin(), report.warnings.end(), [](const auto& warning) {
                    return warning.find("duplicate normalized path") != std::string::npos;
                }), "CSV detects duplicate normalized paths");

    test_support::writeFile(data / "bad-header.csv",
        "artist,title,album,genre,year,duration_sec,file_path\n");
    music_player::MusicLibrary rejected;
    const auto header = music_player::CsvLoader::load(data / "bad-header.csv", rejected);
    test.expect(header.loaded == 0 && !header.warnings.empty(), "CSV requires exact header order");

    test_support::writeFile(data / "reordered.csv",
        "title,artist,album,genre,year,duration_sec,file_path\n"
        "Second,A,B,C,2020,1,music/b.mp3\n"
        "First,A,B,C,2020,1,music/a.mp3\n");
    test_support::writeFile(data / "original.csv",
        "title,artist,album,genre,year,duration_sec,file_path\n"
        "First,A,B,C,2020,1,music/a.mp3\n"
        "Second,A,B,C,2020,1,music/b.mp3\n");
    music_player::MusicLibrary original;
    music_player::MusicLibrary reordered;
    (void)music_player::CsvLoader::load(data / "original.csv", original);
    (void)music_player::CsvLoader::load(data / "reordered.csv", reordered);
    test.expect(original.findByPath(data / "music" / "a.mp3")->get().id()
                    == reordered.findByPath(data / "music" / "a.mp3")->get().id(),
                "song IDs remain stable when CSV rows are reordered");
}

void m3uAndPlaylistTests(Runner& test) {
    test_support::TempDirectory fixture{"playlist"};
    const auto data = fixture.path();
    test_support::writeFile(data / "music" / "a.mp3", "a");
    test_support::writeFile(data / "music" / "b.mp3", "b");
    test_support::writeFile(data / "library.csv",
        "title,artist,album,genre,year,duration_sec,file_path\n"
        "First,A,B,C,2020,10,music/a.mp3\n"
        "Second,A,B,C,2020,20,music/b.mp3\n");
    music_player::MusicLibrary library;
    (void)music_player::CsvLoader::load(data / "library.csv", library);
    test_support::writeFile(data / "Playlists" / "UTF8.M3U8",
        "\xEF\xBB\xBF#EXTM3U\n../music/a.mp3\n../music/missing.mp3\n");
    test_support::writeFile(data / "Playlists" / "empty.M3U", "# only comments\n");
    std::vector<music_player::Playlist> playlists;
    const auto report = music_player::M3uLoader::loadDirectory(data / "Playlists", library, playlists);
    test.expect(report.loaded == 2 && playlists.size() == 2,
                "M3U loader accepts case variants and UTF-8 M3U8 files");
    const auto utf8 = std::find_if(playlists.begin(), playlists.end(), [](const auto& playlist) {
        return playlist.name() == "UTF8";
    });
    test.expect(utf8 != playlists.end() && utf8->songIds().size() == 1,
                "M3U resolves relative tracks and skips missing entries");
    test.expect(std::any_of(report.warnings.begin(), report.warnings.end(), [](const auto& warning) {
                    return warning.find("empty") != std::string::npos;
                }), "empty playlists are retained and reported");

    music_player::PlaylistManager manager{data / "Playlists", library};
    (void)manager.reload();
    std::string error;
    test.expect(manager.create("Road Trip", error), "playlist can be created");
    auto created = manager.playlists().size() - 1;
    test.expect(manager.addTrack(created, library.songs()[0].id(), error)
                    && manager.addTrack(created, library.songs()[1].id(), error),
                "tracks can be added and persisted");
    test.expect(manager.moveTrack(created, 1, 0, error)
                    && manager.playlists()[created].songIds()[0] == library.songs()[1].id(),
                "playlist tracks can be reordered");
    test.expect(manager.removeTrack(created, 1, error), "playlist tracks can be removed");
    test.expect(manager.rename(created, "Driving", error), "playlist can be renamed safely");
    const auto reloaded = manager.reload();
    test.expect(reloaded.loaded >= 3
                    && std::any_of(manager.playlists().begin(), manager.playlists().end(), [](const auto& value) {
                           return value.name() == "Driving" && value.songIds().size() == 1;
                       }), "edited playlist reloads from external-compatible M3U8");
    const auto driving = std::find_if(manager.playlists().begin(), manager.playlists().end(), [](const auto& value) {
        return value.name() == "Driving";
    });
    test.expect(driving != manager.playlists().end()
                    && manager.erase(static_cast<std::size_t>(driving - manager.playlists().begin()), error),
                "playlist can be deleted after confirmation at the UI boundary");
    test.expect(!manager.create("bad/name", error) && !manager.create("", error),
                "invalid playlist filenames are rejected");
}

void configTests(Runner& test) {
    test_support::TempDirectory fixture{"config"};
    const auto file = fixture.path() / "settings.cfg";
    test_support::writeFile(file, "volume=broken\nplayback_mode=SHUFFLE\n");
    const auto malformed = music_player::ConfigManager(file).load();
    test.expect(malformed.volume > 0.79F && malformed.volume < 0.81F
                    && malformed.playbackMode == "SHUFFLE",
                "malformed settings retain safe defaults without discarding valid keys");

    for (const auto stage : {test_support::FailingAtomicOps::Stage::Directory,
                             test_support::FailingAtomicOps::Stage::Temporary,
                             test_support::FailingAtomicOps::Stage::Replacement}) {
        auto operations = std::make_shared<test_support::FailingAtomicOps>(stage);
        music_player::ConfigManager failing{file, operations};
        music_player::AppSettings settings;
        settings.volume = 0.1F;
        std::string error;
        test.expect(!failing.save(settings, error) && !error.empty(),
                    "atomic settings save reports each injected failure");
        test.expect(music_player::ConfigManager(file).load().playbackMode == "SHUFFLE",
                    "failed settings replacement preserves the last known-good file");
        if (stage == test_support::FailingAtomicOps::Stage::Replacement) {
            test.expect(operations->cleaned, "failed replacement cleans its unique temporary file");
        }
    }

    std::vector<std::thread> writers;
    for (int index = 0; index < 8; ++index) {
        writers.emplace_back([file, index] {
            music_player::AppSettings settings;
            settings.playbackMode = index % 2 == 0 ? "REPEAT_ALL" : "SHUFFLE";
            settings.volume = static_cast<float>(index) / 10.0F;
            std::string error;
            (void)music_player::ConfigManager(file).save(settings, error);
        });
    }
    for (auto& writer : writers) writer.join();
    const auto concurrent = music_player::ConfigManager(file).load();
    test.expect((concurrent.playbackMode == "REPEAT_ALL" || concurrent.playbackMode == "SHUFFLE")
                    && concurrent.volume >= 0.0F && concurrent.volume <= 0.7F,
                "concurrent saves leave one complete valid settings document");
}

void utf8AndCliTests(Runner& test) {
    test.expect(music_player::utf8::displayWidth("cafe\xCC\x81") == 4,
                "combining accents do not consume an extra terminal cell");
    test.expect(music_player::utf8::displayWidth("\xF0\x9F\x8E\xB5") == 2,
                "emoji use two terminal cells");
    test.expect(music_player::utf8::displayWidth("\xE9\x9F\xB3\xE4\xB9\x90") == 4,
                "CJK metadata uses wide display cells");
    const auto truncated = music_player::utf8::truncate("\xD9\x85\xD9\x88\xD8\xB3\xDB\x8C\xD9\x82\xDB\x8C", 5);
    test.expect(music_player::utf8::displayWidth(truncated) <= 5
                    && truncated.find('\x80') == std::string::npos,
                "UTF-8 truncation respects code-point boundaries and column limits");

    const auto parsed = music_player::parseCli({"--data-dir", "music data", "--non-interactive", "--no-color"});
    test.expect(parsed && parsed.options.dataDirectory && parsed.options.nonInteractive
                    && parsed.options.noColor,
                "CLI parses supported options and path values");
    test.expect(!music_player::parseCli({"--data-dir"}), "CLI rejects a missing option value");
    test.expect(!music_player::parseCli({"--unknown"}), "CLI rejects unknown options");
    test.expect(music_player::parseCli({"--help"}).options.help, "CLI recognizes help");
}

}  // namespace

int main() {
    Runner test;
    songAndStableIdTests(test);
    libraryIndexTests(test);
    csvTests(test);
    m3uAndPlaylistTests(test);
    configTests(test);
    utf8AndCliTests(test);
    return test.finish("core");
}

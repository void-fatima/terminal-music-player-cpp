#include "Player.h"

#include "TestSupport.h"

#include <memory>
#include <vector>

namespace {

music_player::Song song(music_player::Song::Id id, const char* name, const char* path) {
    return {id, name, "Artist", "Album", "Genre", path,
            music_player::Song::Duration{120000}, 2024};
}

std::vector<music_player::Song> queue() {
    return {song(1, "One", "one.mp3"), song(2, "Two", "two.mp3"), song(3, "Three", "three.mp3")};
}

}  // namespace

int main() {
    using music_player::PlaybackMode;
    using music_player::PlaybackState;
    using music_player::Player;
    test_support::Runner test;
    auto lifetime = std::make_shared<test_support::FakeAudioState>();
    {
        auto backend = std::make_unique<test_support::FakeAudioBackend>(lifetime);
        auto* fake = backend.get();
        Player player{std::move(backend)};

        test.expect(!player.prepare({}, 0) && !player.play(queue(), 9),
                    "empty queues and invalid start indices are rejected");
        test.expect(player.play(queue(), 0) && player.state() == PlaybackState::Playing
                        && player.currentSong()->title() == "One",
                    "successful playback loads and starts the selected track");
        test.expect(player.togglePause() && player.state() == PlaybackState::Paused,
                    "playing track can pause");
        test.expect(player.togglePause() && player.state() == PlaybackState::Playing,
                    "paused track can resume");
        fake->advance(20.0);
        test.expect(player.seekBy(15.0) && fake->position == 35.0,
                    "relative seek updates backend position");
        test.expect(player.setVolume(2.0F) && player.volume() == 1.0F
                        && player.setVolume(-1.0F) && player.volume() == 0.0F,
                    "volume is clamped before reaching the backend");
        test.expect(player.stop() && player.state() == PlaybackState::Stopped
                        && player.queue().size() == 3 && player.currentIndex() == 0,
                    "stop rewinds but preserves queue and selected track");

        test.expect(player.playAt(0) && player.next() && player.currentIndex() == 1,
                    "manual next advances in no-repeat mode");
        test.expect(player.previous() && player.currentIndex() == 0,
                    "previous returns to the prior sequential track");
        test.expect(player.playAt(2) && !player.next() && player.state() == PlaybackState::Stopped,
                    "manual next stops at queue end in no-repeat mode");

        player.setMode(PlaybackMode::RepeatOne);
        test.expect(player.playAt(1), "repeat-one fixture starts");
        const int repeatLoads = lifetime->loads;
        fake->finish();
        test.expect(player.update() && player.currentIndex() == 1 && lifetime->loads == repeatLoads + 1,
                    "repeat one automatically restarts the completed track");

        player.setMode(PlaybackMode::RepeatAll);
        test.expect(player.playAt(2), "repeat-all fixture starts");
        fake->finish();
        test.expect(player.update() && player.currentIndex() == 0,
                    "repeat all automatically wraps at queue end");

        player.setMode(PlaybackMode::Shuffle);
        test.expect(player.playAt(0), "shuffle fixture starts");
        const auto beforeShuffle = player.currentIndex();
        fake->finish();
        test.expect(player.update() && player.currentIndex() != beforeShuffle
                        && !player.shuffleHistory().empty(),
                    "shuffle completion avoids an immediate duplicate and records history");
        test.expect(player.previous() && player.currentIndex() == beforeShuffle,
                    "previous in shuffle returns through real playback history");

        player.clearQueue();
        test.expect(player.enqueue(song(1, "One", "one.mp3"))
                        && player.enqueue(song(2, "Two", "two.mp3"))
                        && player.enqueue(song(3, "Three", "three.mp3")),
                    "songs can be added to a queue");
        test.expect(player.moveInQueue(2, 0) && player.queue()[0].title() == "Three",
                    "queue entries can be reordered");
        test.expect(player.removeFromQueue(1) && player.queue().size() == 2,
                    "queue entries can be removed");
        test.expect(!player.removeFromQueue(8) && !player.moveInQueue(0, 8),
                    "invalid queue edit indices are rejected");
        player.clearQueue();
        test.expect(player.queue().empty() && !player.next(), "clearing produces a safe empty queue");

        fake->failPause = true;
        test.expect(player.play(queue(), 0) && !player.togglePause()
                        && player.lastError().find("pause") != std::string::npos,
                    "pause failures return an actionable backend error");
        fake->failPause = false;
        fake->failSeek = true;
        test.expect(!player.seekBy(1.0) && player.lastError().find("seek") != std::string::npos,
                    "seek failures are not silently ignored");
        fake->failSeek = false;

        auto transitionQueue = std::vector<music_player::Song>{
            song(1, "Good", "good.mp3"), song(2, "Missing", "missing.mp3")};
        player.setMode(PlaybackMode::NoRepeat);
        test.expect(player.play(std::move(transitionQueue), 0), "automatic failure fixture starts");
        fake->finish();
        test.expect(!player.update() && player.currentIndex() == 1
                        && player.state() == PlaybackState::Stopped
                        && player.lastError().find("load") != std::string::npos,
                    "failed automatic transition stops on the unreadable track with an error");

        fake->failPlay = true;
        test.expect(!player.play(queue(), 0) && player.lastError().find("play") != std::string::npos,
                    "failed playback start returns an error");
        fake->failPlay = false;
        test.expect(player.play(queue(), 0) && player.lastError().empty(),
                    "successful playback clears stale errors at a state boundary");
    }
    test.expect(lifetime->destroyed == 1, "audio backend resources are released exactly once");
    return test.finish("player");
}

#include "Player.h"
#include "StableId.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: terminal-music-player-audio-smoke AUDIO_FILE\n";
        return 2;
    }
    const std::filesystem::path file = argv[1];
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        std::cerr << "Audio fixture is not readable: " << file.string() << '\n';
        return 2;
    }
    music_player::Player player;
    std::vector<music_player::Song> queue;
    queue.emplace_back(music_player::stableSongId(file), file.stem().string(), "Smoke Test", "",
                       "", file, music_player::Song::Duration{5000}, 0);
    if (!player.play(std::move(queue))) {
        std::cerr << player.lastError() << '\n';
        return 1;
    }
    std::cout << "Playing five-second real-device smoke test: " << file.string() << '\n';
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (!player.stop()) {
        std::cerr << player.lastError() << '\n';
        return 1;
    }
    std::cout << "Audio smoke test completed. Confirm audibility manually.\n";
    return 0;
}

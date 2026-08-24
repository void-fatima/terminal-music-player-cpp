#pragma once

#include "Song.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace music_player {

enum class PlaybackMode { NoRepeat, RepeatOne, RepeatAll, Shuffle };
enum class PlaybackState { Stopped, Playing, Paused };

class Player {
public:
    Player();
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    bool prepare(std::vector<Song> queue, std::size_t startIndex = 0);
    bool play(std::vector<Song> queue, std::size_t startIndex = 0);
    bool togglePause();
    void stop();
    bool next();
    bool previous();
    bool seekBy(double seconds);
    void update();

    void setMode(PlaybackMode mode) noexcept;
    PlaybackMode mode() const noexcept;
    void setVolume(float volume) noexcept;
    float volume() const noexcept;

    PlaybackState state() const noexcept;
    const Song* currentSong() const noexcept;
    std::size_t currentIndex() const noexcept;
    const std::vector<Song>& queue() const noexcept;
    double positionSeconds() const noexcept;
    double durationSeconds() const noexcept;
    bool audioAvailable() const noexcept;
    const std::string& lastError() const noexcept;

    static const char* modeName(PlaybackMode mode) noexcept;
    static PlaybackMode parseMode(const std::string& value) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace music_player

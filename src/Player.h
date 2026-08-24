#pragma once

#include "IAudioBackend.h"
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
    explicit Player(std::unique_ptr<IAudioBackend> backend = {});
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    bool prepare(std::vector<Song> queue, std::size_t startIndex = 0);
    bool play(std::vector<Song> queue, std::size_t startIndex = 0);
    bool togglePause();
    bool stop();
    bool next();
    bool previous();
    bool seekBy(double seconds);
    bool update();

    bool playAt(std::size_t index);
    bool enqueue(Song song);
    bool removeFromQueue(std::size_t index);
    bool moveInQueue(std::size_t from, std::size_t to);
    void clearQueue();

    void setMode(PlaybackMode mode) noexcept;
    PlaybackMode mode() const noexcept;
    bool setVolume(float volume);
    float volume() const noexcept;

    PlaybackState state() const noexcept;
    const Song* currentSong() const noexcept;
    std::size_t currentIndex() const noexcept;
    const std::vector<Song>& queue() const noexcept;
    double positionSeconds() const noexcept;
    double durationSeconds() const noexcept;
    bool audioAvailable() const noexcept;
    const std::string& lastError() const noexcept;
    const std::vector<std::size_t>& shuffleHistory() const noexcept;

    static const char* modeName(PlaybackMode mode) noexcept;
    static PlaybackMode parseMode(const std::string& value) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace music_player

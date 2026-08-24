#pragma once

#include <filesystem>
#include <string>

namespace music_player {

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual bool load(const std::filesystem::path& file) = 0;
    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool stop() = 0;
    virtual bool seek(double seconds) = 0;
    virtual bool setVolume(float volume) = 0;

    virtual bool isAvailable() const noexcept = 0;
    virtual bool isLoaded() const noexcept = 0;
    virtual bool isAtEnd() const noexcept = 0;
    virtual double positionSeconds() const noexcept = 0;
    virtual double durationSeconds() const noexcept = 0;
    virtual const std::string& lastError() const noexcept = 0;
};

}  // namespace music_player

#pragma once

#include "IAudioBackend.h"

#include <memory>

namespace music_player {

class MiniaudioBackend final : public IAudioBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend() override;
    MiniaudioBackend(const MiniaudioBackend&) = delete;
    MiniaudioBackend& operator=(const MiniaudioBackend&) = delete;

    bool load(const std::filesystem::path& file) override;
    bool play() override;
    bool pause() override;
    bool stop() override;
    bool seek(double seconds) override;
    bool setVolume(float volume) override;

    bool isAvailable() const noexcept override;
    bool isLoaded() const noexcept override;
    bool isAtEnd() const noexcept override;
    double positionSeconds() const noexcept override;
    double durationSeconds() const noexcept override;
    const std::string& lastError() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::unique_ptr<IAudioBackend> makeMiniaudioBackend();

}  // namespace music_player

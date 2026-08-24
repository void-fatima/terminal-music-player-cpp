#define MINIAUDIO_IMPLEMENTATION
#include "../third_party/miniaudio/miniaudio.h"

#include "MiniaudioBackend.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace music_player {
namespace {

std::string describe(const char* action, ma_result result) {
    return std::string(action) + ": " + ma_result_description(result);
}

}  // namespace

struct MiniaudioBackend::Impl {
    ma_engine engine{};
    ma_sound sound{};
    bool engineInitialized{false};
    bool soundInitialized{false};
    float volume{0.8F};
    std::string error;

    Impl() {
        const ma_result result = ma_engine_init(nullptr, &engine);
        if (result != MA_SUCCESS) {
            error = describe("Audio engine unavailable", result);
            return;
        }
        engineInitialized = true;
        const ma_result volumeResult = ma_engine_set_volume(&engine, volume);
        if (volumeResult != MA_SUCCESS) {
            error = describe("Cannot set engine volume", volumeResult);
        }
    }

    ~Impl() {
        unload();
        if (engineInitialized) ma_engine_uninit(&engine);
    }

    void unload() noexcept {
        if (!soundInitialized) return;
        (void)ma_sound_stop(&sound);
        ma_sound_uninit(&sound);
        soundInitialized = false;
    }
};

MiniaudioBackend::MiniaudioBackend() : impl_(std::make_unique<Impl>()) {}
MiniaudioBackend::~MiniaudioBackend() = default;

bool MiniaudioBackend::load(const std::filesystem::path& file) {
    impl_->unload();
    if (!impl_->engineInitialized) {
        if (impl_->error.empty()) impl_->error = "Audio engine is unavailable.";
        return false;
    }

    ma_result result = MA_ERROR;
#ifdef _WIN32
    result = ma_sound_init_from_file_w(
        &impl_->engine, file.c_str(), MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
        nullptr, nullptr, &impl_->sound);
#else
    const auto nativePath = file.string();
    result = ma_sound_init_from_file(
        &impl_->engine, nativePath.c_str(),
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
        nullptr, nullptr, &impl_->sound);
#endif
    if (result != MA_SUCCESS) {
        impl_->error = "Cannot open '" + file.string() + "': " + ma_result_description(result);
        return false;
    }

    impl_->soundInitialized = true;
    ma_sound_set_volume(&impl_->sound, impl_->volume);
    impl_->error.clear();
    return true;
}

bool MiniaudioBackend::play() {
    if (!impl_->soundInitialized) {
        impl_->error = "No audio file is loaded.";
        return false;
    }
    const ma_result result = ma_sound_start(&impl_->sound);
    if (result != MA_SUCCESS) {
        impl_->error = describe("Cannot start playback", result);
        return false;
    }
    impl_->error.clear();
    return true;
}

bool MiniaudioBackend::pause() {
    if (!impl_->soundInitialized) {
        impl_->error = "No audio file is loaded.";
        return false;
    }
    const ma_result result = ma_sound_stop(&impl_->sound);
    if (result != MA_SUCCESS) {
        impl_->error = describe("Cannot pause playback", result);
        return false;
    }
    impl_->error.clear();
    return true;
}

bool MiniaudioBackend::stop() {
    if (!impl_->soundInitialized) {
        impl_->error.clear();
        return true;
    }
    const ma_result result = ma_sound_stop(&impl_->sound);
    if (result != MA_SUCCESS) {
        impl_->error = describe("Cannot stop playback", result);
        return false;
    }
    const ma_result seekResult = ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
    if (seekResult != MA_SUCCESS) {
        impl_->error = describe("Cannot rewind stopped track", seekResult);
        return false;
    }
    impl_->error.clear();
    return true;
}

bool MiniaudioBackend::seek(double seconds) {
    if (!impl_->soundInitialized || !std::isfinite(seconds)) {
        impl_->error = "Cannot seek without a loaded track.";
        return false;
    }
    const ma_result result = ma_sound_seek_to_second(
        &impl_->sound, static_cast<float>(std::max(0.0, seconds)));
    if (result != MA_SUCCESS) {
        impl_->error = describe("Cannot seek", result);
        return false;
    }
    impl_->error.clear();
    return true;
}

bool MiniaudioBackend::setVolume(float volume) {
    impl_->volume = std::clamp(volume, 0.0F, 1.0F);
    if (!impl_->engineInitialized) {
        if (impl_->error.empty()) impl_->error = "Audio engine is unavailable.";
        return false;
    }
    const ma_result result = ma_engine_set_volume(&impl_->engine, impl_->volume);
    if (result != MA_SUCCESS) {
        impl_->error = describe("Cannot set volume", result);
        return false;
    }
    if (impl_->soundInitialized) ma_sound_set_volume(&impl_->sound, impl_->volume);
    impl_->error.clear();
    return true;
}

bool MiniaudioBackend::isAvailable() const noexcept { return impl_->engineInitialized; }
bool MiniaudioBackend::isLoaded() const noexcept { return impl_->soundInitialized; }
bool MiniaudioBackend::isAtEnd() const noexcept {
    return impl_->soundInitialized && ma_sound_at_end(&impl_->sound) == MA_TRUE;
}

double MiniaudioBackend::positionSeconds() const noexcept {
    float position = 0.0F;
    if (!impl_->soundInitialized
        || ma_sound_get_cursor_in_seconds(&impl_->sound, &position) != MA_SUCCESS
        || !std::isfinite(position)) return 0.0;
    return position;
}

double MiniaudioBackend::durationSeconds() const noexcept {
    float duration = 0.0F;
    if (!impl_->soundInitialized
        || ma_sound_get_length_in_seconds(&impl_->sound, &duration) != MA_SUCCESS
        || !std::isfinite(duration)) return 0.0;
    return duration;
}

const std::string& MiniaudioBackend::lastError() const noexcept { return impl_->error; }

std::unique_ptr<IAudioBackend> makeMiniaudioBackend() {
    return std::make_unique<MiniaudioBackend>();
}

}  // namespace music_player

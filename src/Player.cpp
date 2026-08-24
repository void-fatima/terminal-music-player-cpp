#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "Player.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <random>
#include <utility>

namespace music_player {

struct Player::Impl {
    ma_engine engine{};
    ma_sound sound{};
    bool engineInitialized{false};
    bool soundInitialized{false};
    std::vector<Song> queue;
    std::size_t index{0};
    PlaybackMode mode{PlaybackMode::NoRepeat};
    PlaybackState state{PlaybackState::Stopped};
    float volume{0.8F};
    std::string error;
    std::mt19937 random{static_cast<std::mt19937::result_type>(
        std::chrono::steady_clock::now().time_since_epoch().count())};

    Impl() {
        const ma_result result = ma_engine_init(nullptr, &engine);
        if (result == MA_SUCCESS) {
            engineInitialized = true;
            ma_engine_set_volume(&engine, volume);
        } else {
            error = std::string("Audio engine unavailable: ") + ma_result_description(result);
        }
    }

    ~Impl() {
        unload();
        if (engineInitialized) ma_engine_uninit(&engine);
    }

    void unload() {
        if (soundInitialized) {
            ma_sound_stop(&sound);
            ma_sound_uninit(&sound);
            soundInitialized = false;
        }
    }

    bool startCurrent() {
        unload();
        if (!engineInitialized) {
            state = PlaybackState::Stopped;
            return false;
        }
        if (queue.empty() || index >= queue.size()) {
            error = "The playback queue is empty.";
            state = PlaybackState::Stopped;
            return false;
        }
        ma_result initialized;
#ifdef _WIN32
        initialized = ma_sound_init_from_file_w(
            &engine, queue[index].filePath().c_str(),
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
            nullptr, nullptr, &sound);
#else
        const auto path = queue[index].filePath().string();
        initialized = ma_sound_init_from_file(
            &engine, path.c_str(), MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
            nullptr, nullptr, &sound);
#endif
        if (initialized != MA_SUCCESS) {
            error = "Cannot open '" + queue[index].filePath().string() + "': "
                + ma_result_description(initialized);
            state = PlaybackState::Stopped;
            return false;
        }
        soundInitialized = true;
        ma_sound_set_volume(&sound, volume);
        const ma_result started = ma_sound_start(&sound);
        if (started != MA_SUCCESS) {
            error = std::string("Cannot start playback: ") + ma_result_description(started);
            unload();
            state = PlaybackState::Stopped;
            return false;
        }
        state = PlaybackState::Playing;
        error.clear();
        return true;
    }

    bool advance(bool automatic) {
        if (queue.empty()) return false;
        if (automatic && mode == PlaybackMode::RepeatOne) return startCurrent();
        if (mode == PlaybackMode::Shuffle && queue.size() > 1) {
            std::uniform_int_distribution<std::size_t> distribution(0, queue.size() - 2);
            auto candidate = distribution(random);
            if (candidate >= index) ++candidate;
            index = candidate;
        } else if (index + 1 < queue.size()) {
            ++index;
        } else if (mode == PlaybackMode::RepeatAll || mode == PlaybackMode::Shuffle) {
            index = 0;
        } else {
            unload();
            state = PlaybackState::Stopped;
            error.clear();
            return false;
        }
        return startCurrent();
    }
};

Player::Player() : impl_(std::make_unique<Impl>()) {}
Player::~Player() = default;

bool Player::prepare(std::vector<Song> queue, std::size_t startIndex) {
    if (queue.empty() || startIndex >= queue.size()) {
        impl_->error = "Select a valid song before playing.";
        return false;
    }
    impl_->unload();
    impl_->queue = std::move(queue);
    impl_->index = startIndex;
    impl_->state = PlaybackState::Stopped;
    impl_->error.clear();
    return true;
}

bool Player::play(std::vector<Song> queue, std::size_t startIndex) {
    if (!prepare(std::move(queue), startIndex)) return false;
    return impl_->startCurrent();
}

bool Player::togglePause() {
    if (!impl_->soundInitialized) {
        if (impl_->state == PlaybackState::Stopped && !impl_->queue.empty()) {
            return impl_->startCurrent();
        }
        impl_->error = "Nothing is loaded.";
        return false;
    }
    if (impl_->state == PlaybackState::Playing) {
        if (ma_sound_stop(&impl_->sound) != MA_SUCCESS) return false;
        impl_->state = PlaybackState::Paused;
    } else if (impl_->state == PlaybackState::Paused) {
        if (ma_sound_start(&impl_->sound) != MA_SUCCESS) return false;
        impl_->state = PlaybackState::Playing;
    } else {
        return impl_->startCurrent();
    }
    impl_->error.clear();
    return true;
}

void Player::stop() {
    impl_->unload();
    impl_->state = PlaybackState::Stopped;
}

bool Player::next() { return impl_->advance(false); }

bool Player::previous() {
    if (impl_->queue.empty()) return false;
    if (positionSeconds() > 3.0 && impl_->soundInitialized) {
        ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
        return true;
    }
    if (impl_->index > 0) --impl_->index;
    else if (impl_->mode == PlaybackMode::RepeatAll) impl_->index = impl_->queue.size() - 1;
    else impl_->index = 0;
    return impl_->startCurrent();
}

bool Player::seekBy(double seconds) {
    if (!impl_->soundInitialized) return false;
    const double target = std::clamp(positionSeconds() + seconds, 0.0, durationSeconds());
    return ma_sound_seek_to_second(&impl_->sound, static_cast<float>(target)) == MA_SUCCESS;
}

void Player::update() {
    if (impl_->state == PlaybackState::Playing && impl_->soundInitialized
        && ma_sound_at_end(&impl_->sound)) {
        impl_->advance(true);
    }
}

void Player::setMode(PlaybackMode mode) noexcept { impl_->mode = mode; }
PlaybackMode Player::mode() const noexcept { return impl_->mode; }

void Player::setVolume(float volume) noexcept {
    impl_->volume = std::clamp(volume, 0.0F, 1.0F);
    if (impl_->engineInitialized) ma_engine_set_volume(&impl_->engine, impl_->volume);
    if (impl_->soundInitialized) ma_sound_set_volume(&impl_->sound, impl_->volume);
}

float Player::volume() const noexcept { return impl_->volume; }
PlaybackState Player::state() const noexcept { return impl_->state; }

const Song* Player::currentSong() const noexcept {
    return impl_->queue.empty() || impl_->index >= impl_->queue.size() ? nullptr : &impl_->queue[impl_->index];
}

std::size_t Player::currentIndex() const noexcept { return impl_->index; }
const std::vector<Song>& Player::queue() const noexcept { return impl_->queue; }

double Player::positionSeconds() const noexcept {
    float position = 0.0F;
    if (impl_->soundInitialized) ma_sound_get_cursor_in_seconds(&impl_->sound, &position);
    return position;
}

double Player::durationSeconds() const noexcept {
    float duration = 0.0F;
    if (impl_->soundInitialized && ma_sound_get_length_in_seconds(&impl_->sound, &duration) == MA_SUCCESS
        && std::isfinite(duration)) return duration;
    const Song* song = currentSong();
    return song ? static_cast<double>(song->duration().count()) / 1000.0 : 0.0;
}

bool Player::audioAvailable() const noexcept { return impl_->engineInitialized; }
const std::string& Player::lastError() const noexcept { return impl_->error; }

const char* Player::modeName(PlaybackMode mode) noexcept {
    switch (mode) {
        case PlaybackMode::NoRepeat: return "NO_REPEAT";
        case PlaybackMode::RepeatOne: return "REPEAT_ONE";
        case PlaybackMode::RepeatAll: return "REPEAT_ALL";
        case PlaybackMode::Shuffle: return "SHUFFLE";
    }
    return "NO_REPEAT";
}

PlaybackMode Player::parseMode(const std::string& value) noexcept {
    if (value == "REPEAT_ONE") return PlaybackMode::RepeatOne;
    if (value == "REPEAT_ALL") return PlaybackMode::RepeatAll;
    if (value == "SHUFFLE") return PlaybackMode::Shuffle;
    return PlaybackMode::NoRepeat;
}

}  // namespace music_player

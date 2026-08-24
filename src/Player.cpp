#include "Player.h"

#include "MiniaudioBackend.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <utility>

namespace music_player {

struct Player::Impl {
    explicit Impl(std::unique_ptr<IAudioBackend> selectedBackend)
        : backend(selectedBackend ? std::move(selectedBackend) : makeMiniaudioBackend()),
          random(static_cast<std::mt19937::result_type>(
              std::chrono::steady_clock::now().time_since_epoch().count())) {}

    std::unique_ptr<IAudioBackend> backend;
    std::vector<Song> queue;
    std::vector<std::size_t> history;
    std::size_t index{0};
    PlaybackMode mode{PlaybackMode::NoRepeat};
    PlaybackState state{PlaybackState::Stopped};
    float volume{0.8F};
    std::string error;
    std::mt19937 random;

    bool fail(std::string fallback) {
        error = backend->lastError().empty() ? std::move(fallback) : backend->lastError();
        return false;
    }

    bool startCurrent() {
        if (queue.empty() || index >= queue.size()) {
            state = PlaybackState::Stopped;
            error = "The playback queue is empty.";
            return false;
        }
        if (!backend->load(queue[index].filePath())) {
            state = PlaybackState::Stopped;
            return fail("Cannot load the selected track.");
        }
        if (!backend->setVolume(volume)) {
            state = PlaybackState::Stopped;
            return fail("Cannot apply the playback volume.");
        }
        if (!backend->play()) {
            state = PlaybackState::Stopped;
            return fail("Cannot start playback.");
        }
        state = PlaybackState::Playing;
        error.clear();
        return true;
    }

    bool chooseShuffleNext() {
        if (queue.size() <= 1) return !queue.empty();
        std::uniform_int_distribution<std::size_t> distribution(0, queue.size() - 2);
        auto candidate = distribution(random);
        if (candidate >= index) ++candidate;
        history.push_back(index);
        index = candidate;
        return true;
    }

    bool advance(bool automatic) {
        if (queue.empty()) {
            error = "The playback queue is empty.";
            return false;
        }
        if (automatic && mode == PlaybackMode::RepeatOne) return startCurrent();
        if (mode == PlaybackMode::Shuffle) {
            if (!chooseShuffleNext()) return false;
        } else if (index + 1 < queue.size()) {
            ++index;
        } else if (mode == PlaybackMode::RepeatAll) {
            index = 0;
        } else {
            if (!backend->stop()) return fail("Cannot stop at the end of the queue.");
            state = PlaybackState::Stopped;
            if (automatic) {
                error.clear();
                return true;
            }
            error = "Already at the end of the queue.";
            return false;
        }
        return startCurrent();
    }
};

Player::Player(std::unique_ptr<IAudioBackend> backend)
    : impl_(std::make_unique<Impl>(std::move(backend))) {
    if (!impl_->backend->isAvailable()) {
        impl_->error = impl_->backend->lastError();
    } else if (!impl_->backend->setVolume(impl_->volume)) {
        impl_->error = impl_->backend->lastError();
    }
}

Player::~Player() = default;

bool Player::prepare(std::vector<Song> queue, std::size_t startIndex) {
    if (queue.empty() || startIndex >= queue.size()) {
        impl_->error = "Select a valid song before playing.";
        return false;
    }
    if (!impl_->backend->stop()) return impl_->fail("Cannot stop the current track.");
    impl_->queue = std::move(queue);
    impl_->index = startIndex;
    impl_->history.clear();
    impl_->state = PlaybackState::Stopped;
    impl_->error.clear();
    return true;
}

bool Player::play(std::vector<Song> queue, std::size_t startIndex) {
    return prepare(std::move(queue), startIndex) && impl_->startCurrent();
}

bool Player::playAt(std::size_t index) {
    if (index >= impl_->queue.size()) {
        impl_->error = "Queue index is out of range.";
        return false;
    }
    if (impl_->mode == PlaybackMode::Shuffle && index != impl_->index && !impl_->queue.empty()) {
        impl_->history.push_back(impl_->index);
    }
    impl_->index = index;
    return impl_->startCurrent();
}

bool Player::togglePause() {
    if (impl_->state == PlaybackState::Playing) {
        if (!impl_->backend->pause()) return impl_->fail("Cannot pause playback.");
        impl_->state = PlaybackState::Paused;
        impl_->error.clear();
        return true;
    }
    if (impl_->state == PlaybackState::Paused) {
        if (!impl_->backend->play()) return impl_->fail("Cannot resume playback.");
        impl_->state = PlaybackState::Playing;
        impl_->error.clear();
        return true;
    }
    if (!impl_->queue.empty()) return impl_->startCurrent();
    impl_->error = "Nothing is loaded.";
    return false;
}

bool Player::stop() {
    if (!impl_->backend->stop()) return impl_->fail("Cannot stop playback.");
    impl_->state = PlaybackState::Stopped;
    impl_->error.clear();
    return true;
}

bool Player::next() { return impl_->advance(false); }

bool Player::previous() {
    if (impl_->queue.empty()) {
        impl_->error = "The playback queue is empty.";
        return false;
    }
    if (positionSeconds() > 3.0) return seekBy(-positionSeconds());
    if (impl_->mode == PlaybackMode::Shuffle && !impl_->history.empty()) {
        impl_->index = impl_->history.back();
        impl_->history.pop_back();
    } else if (impl_->index > 0) {
        --impl_->index;
    } else if (impl_->mode == PlaybackMode::RepeatAll) {
        impl_->index = impl_->queue.size() - 1;
    }
    return impl_->startCurrent();
}

bool Player::seekBy(double seconds) {
    if (!impl_->backend->isLoaded() || !std::isfinite(seconds)) {
        impl_->error = "Cannot seek without a loaded track.";
        return false;
    }
    const double duration = durationSeconds();
    const double upper = duration > 0.0 ? duration : positionSeconds() + std::max(0.0, seconds);
    const double target = std::clamp(positionSeconds() + seconds, 0.0, upper);
    if (!impl_->backend->seek(target)) return impl_->fail("Cannot seek in the current track.");
    impl_->error.clear();
    return true;
}

bool Player::update() {
    if (impl_->state != PlaybackState::Playing || !impl_->backend->isAtEnd()) return true;
    return impl_->advance(true);
}

bool Player::enqueue(Song song) {
    impl_->queue.push_back(std::move(song));
    if (impl_->queue.size() == 1) impl_->index = 0;
    impl_->error.clear();
    return true;
}

bool Player::removeFromQueue(std::size_t index) {
    if (index >= impl_->queue.size()) {
        impl_->error = "Queue index is out of range.";
        return false;
    }
    if (index == impl_->index && !stop()) return false;
    impl_->queue.erase(impl_->queue.begin() + static_cast<std::ptrdiff_t>(index));
    impl_->history.clear();
    if (impl_->queue.empty()) impl_->index = 0;
    else if (index < impl_->index) --impl_->index;
    else if (impl_->index >= impl_->queue.size()) impl_->index = impl_->queue.size() - 1;
    impl_->error.clear();
    return true;
}

bool Player::moveInQueue(std::size_t from, std::size_t to) {
    if (from >= impl_->queue.size() || to >= impl_->queue.size()) {
        impl_->error = "Queue index is out of range.";
        return false;
    }
    if (from == to) {
        impl_->error.clear();
        return true;
    }
    Song moving = std::move(impl_->queue[from]);
    impl_->queue.erase(impl_->queue.begin() + static_cast<std::ptrdiff_t>(from));
    impl_->queue.insert(impl_->queue.begin() + static_cast<std::ptrdiff_t>(to), std::move(moving));
    if (impl_->index == from) impl_->index = to;
    else if (from < impl_->index && to >= impl_->index) --impl_->index;
    else if (from > impl_->index && to <= impl_->index) ++impl_->index;
    impl_->history.clear();
    impl_->error.clear();
    return true;
}

void Player::clearQueue() {
    (void)stop();
    impl_->queue.clear();
    impl_->history.clear();
    impl_->index = 0;
}

void Player::setMode(PlaybackMode mode) noexcept {
    impl_->mode = mode;
    if (mode != PlaybackMode::Shuffle) impl_->history.clear();
}

PlaybackMode Player::mode() const noexcept { return impl_->mode; }

bool Player::setVolume(float volume) {
    impl_->volume = std::clamp(volume, 0.0F, 1.0F);
    if (!impl_->backend->setVolume(impl_->volume)) return impl_->fail("Cannot set playback volume.");
    impl_->error.clear();
    return true;
}

float Player::volume() const noexcept { return impl_->volume; }
PlaybackState Player::state() const noexcept { return impl_->state; }

const Song* Player::currentSong() const noexcept {
    return impl_->queue.empty() || impl_->index >= impl_->queue.size()
        ? nullptr : &impl_->queue[impl_->index];
}

std::size_t Player::currentIndex() const noexcept { return impl_->index; }
const std::vector<Song>& Player::queue() const noexcept { return impl_->queue; }
double Player::positionSeconds() const noexcept { return impl_->backend->positionSeconds(); }

double Player::durationSeconds() const noexcept {
    const double backendDuration = impl_->backend->durationSeconds();
    if (backendDuration > 0.0) return backendDuration;
    const Song* song = currentSong();
    return song ? static_cast<double>(song->duration().count()) / 1000.0 : 0.0;
}

bool Player::audioAvailable() const noexcept { return impl_->backend->isAvailable(); }
const std::string& Player::lastError() const noexcept { return impl_->error; }
const std::vector<std::size_t>& Player::shuffleHistory() const noexcept { return impl_->history; }

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

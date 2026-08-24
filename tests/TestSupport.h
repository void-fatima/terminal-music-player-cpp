#pragma once

#include "AtomicFile.h"
#include "IAudioBackend.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace test_support {

class Runner {
public:
    void expect(bool condition, const std::string& message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures_;
        }
    }
    int finish(const char* suite) const {
        if (failures_ == 0) std::cout << "All " << suite << " tests passed.\n";
        return failures_ == 0 ? 0 : 1;
    }
private:
    int failures_{0};
};

class TempDirectory {
public:
    explicit TempDirectory(const std::string& label) {
        static std::atomic<unsigned long long> counter{0};
#ifdef _WIN32
        const auto process = static_cast<unsigned long>(_getpid());
#else
        const auto process = static_cast<unsigned long>(getpid());
#endif
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("terminal-music-player-" + label + "-" + std::to_string(process) + "-"
               + std::to_string(stamp) + "-" + std::to_string(counter.fetch_add(1)));
        std::error_code error;
        std::filesystem::create_directories(path_, error);
        if (error) throw std::filesystem::filesystem_error("create test fixture", path_, error);
    }
    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    const std::filesystem::path& path() const noexcept { return path_; }
private:
    std::filesystem::path path_;
};

inline void writeFile(const std::filesystem::path& file, const std::string& contents) {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    output << contents;
    if (!output) throw std::runtime_error("Cannot write test fixture: " + file.string());
}

struct FakeAudioState {
    int destroyed{0};
    int loads{0};
    int plays{0};
    int pauses{0};
    int stops{0};
};

class FakeAudioBackend final : public music_player::IAudioBackend {
public:
    explicit FakeAudioBackend(std::shared_ptr<FakeAudioState> state = std::make_shared<FakeAudioState>())
        : sharedState(std::move(state)) {}
    ~FakeAudioBackend() override { ++sharedState->destroyed; }

    bool load(const std::filesystem::path& file) override {
        ++sharedState->loads;
        if (failLoad || file.filename().string().find("missing") != std::string::npos) return fail("fake load failure");
        file_ = file;
        loaded = true;
        ended = false;
        position = 0.0;
        error.clear();
        return true;
    }
    bool play() override {
        ++sharedState->plays;
        if (failPlay || !loaded) return fail("fake play failure");
        playing = true;
        error.clear();
        return true;
    }
    bool pause() override {
        ++sharedState->pauses;
        if (failPause || !loaded) return fail("fake pause failure");
        playing = false;
        error.clear();
        return true;
    }
    bool stop() override {
        ++sharedState->stops;
        if (failStop) return fail("fake stop failure");
        playing = false;
        position = 0.0;
        error.clear();
        return true;
    }
    bool seek(double seconds) override {
        if (failSeek || !loaded) return fail("fake seek failure");
        position = seconds;
        ended = false;
        error.clear();
        return true;
    }
    bool setVolume(float value) override {
        if (failVolume) return fail("fake volume failure");
        volume = value;
        error.clear();
        return true;
    }
    bool isAvailable() const noexcept override { return available; }
    bool isLoaded() const noexcept override { return loaded; }
    bool isAtEnd() const noexcept override { return ended; }
    double positionSeconds() const noexcept override { return position; }
    double durationSeconds() const noexcept override { return duration; }
    const std::string& lastError() const noexcept override { return error; }

    void finish() { ended = true; position = duration; playing = false; }
    void advance(double seconds) { position += seconds; }
    bool fail(std::string message) { error = std::move(message); return false; }

    std::shared_ptr<FakeAudioState> sharedState;
    bool available{true};
    bool loaded{false};
    bool playing{false};
    bool ended{false};
    bool failLoad{false};
    bool failPlay{false};
    bool failPause{false};
    bool failStop{false};
    bool failSeek{false};
    bool failVolume{false};
    double position{0.0};
    double duration{120.0};
    float volume{0.8F};
    std::filesystem::path file_;
    std::string error;
};

class FailingAtomicOps final : public music_player::IAtomicFileOps {
public:
    enum class Stage { None, Directory, Temporary, Replacement };
    explicit FailingAtomicOps(Stage failure) : failure_(failure) {}
    bool createParentDirectories(const std::filesystem::path&, std::string& error) override {
        if (failure_ == Stage::Directory) { error = "injected directory failure"; return false; }
        return true;
    }
    bool writeUniqueTemporary(const std::filesystem::path& target,
                              const std::string& contents,
                              std::filesystem::path& temporary,
                              std::string& error) override {
        if (failure_ == Stage::Temporary) { error = "injected write failure"; return false; }
        temporary = target.string() + ".injected.tmp";
        savedContents = contents;
        return true;
    }
    bool replaceFile(const std::filesystem::path&,
                     const std::filesystem::path&,
                     std::string& error) override {
        if (failure_ == Stage::Replacement) { error = "injected replacement failure"; return false; }
        return true;
    }
    void removeFile(const std::filesystem::path&) noexcept override { cleaned = true; }

    Stage failure_;
    std::string savedContents;
    bool cleaned{false};
};

}  // namespace test_support

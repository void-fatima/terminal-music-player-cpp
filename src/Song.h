#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace music_player {

class Song {
public:
    using Id = std::uint64_t;
    using Duration = std::chrono::milliseconds;

    Song(Id id,
         std::string title,
         std::string artist,
         std::string album,
         std::string genre,
         std::filesystem::path filePath,
         Duration duration,
         int year = 0);

    Id id() const noexcept;
    const std::string& title() const noexcept;
    const std::string& artist() const noexcept;
    const std::string& album() const noexcept;
    const std::string& genre() const noexcept;
    const std::filesystem::path& filePath() const noexcept;
    Duration duration() const noexcept;
    int year() const noexcept;

private:
    Id id_;
    std::string title_;
    std::string artist_;
    std::string album_;
    std::string genre_;
    std::filesystem::path filePath_;
    Duration duration_;
    int year_;
};

}  // namespace music_player

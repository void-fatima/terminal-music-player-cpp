#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace music_player {

struct CliOptions {
    std::optional<std::filesystem::path> dataDirectory;
    bool help{false};
    bool nonInteractive{false};
    bool noColor{false};
    bool snapshot{false};
};

struct CliParseResult {
    CliOptions options;
    std::string error;
    explicit operator bool() const noexcept { return error.empty(); }
};

CliParseResult parseCli(const std::vector<std::string>& arguments);
std::filesystem::path executablePath(const char* argv0);
std::filesystem::path resolveDataDirectory(const CliOptions& options, const char* argv0);
bool standardStreamsAreTerminals() noexcept;
std::string usage();

}  // namespace music_player

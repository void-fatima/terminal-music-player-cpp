#include "CliOptions.h"

#include "PathUtils.h"

#include <cstdlib>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace music_player {
namespace {

bool isDirectory(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

std::filesystem::path fromPathEnvironment(const std::filesystem::path& executable) {
    const char* raw = std::getenv("PATH");
    if (raw == nullptr) return {};
#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    std::stringstream paths(raw);
    std::string directory;
    while (std::getline(paths, directory, separator)) {
        if (directory.empty()) continue;
        auto candidate = std::filesystem::path(directory) / executable;
#ifdef _WIN32
        if (!candidate.has_extension()) candidate += ".exe";
#endif
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) {
            return std::filesystem::absolute(candidate, error);
        }
    }
    return {};
}

}  // namespace

CliParseResult parseCli(const std::vector<std::string>& arguments) {
    CliParseResult result;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];
        if (argument == "--help" || argument == "-h") result.options.help = true;
        else if (argument == "--non-interactive") result.options.nonInteractive = true;
        else if (argument == "--no-color") result.options.noColor = true;
        else if (argument == "--snapshot") result.options.snapshot = true;
        else if (argument == "--data-dir") {
            if (index + 1 >= arguments.size() || arguments[index + 1].rfind("--", 0) == 0) {
                result.error = "--data-dir requires a path value.";
                return result;
            }
            if (result.options.dataDirectory) {
                result.error = "--data-dir may be specified only once.";
                return result;
            }
            result.options.dataDirectory = pathFromUtf8(arguments[++index]);
        } else {
            result.error = "Unknown option: " + argument;
            return result;
        }
    }
    return result;
}

std::filesystem::path executablePath(const char* argv0) {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        buffer.resize(length);
        return std::filesystem::path(buffer);
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    (void)_NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        std::error_code error;
        return std::filesystem::weakly_canonical(buffer.c_str(), error);
    }
#else
    std::string buffer(4096, '\0');
    const auto length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length > 0 && static_cast<std::size_t>(length) < buffer.size()) {
        buffer.resize(static_cast<std::size_t>(length));
        return std::filesystem::path(buffer);
    }
#endif
    if (argv0 == nullptr || *argv0 == '\0') return {};
    const std::filesystem::path supplied = pathFromUtf8(argv0);
    std::error_code error;
    if (supplied.has_parent_path()) return std::filesystem::absolute(supplied, error);
    return fromPathEnvironment(supplied);
}

std::filesystem::path resolveDataDirectory(const CliOptions& options, const char* argv0) {
    if (options.dataDirectory) return options.dataDirectory->lexically_normal();
    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (!error && isDirectory(current / "Data")) return current / "Data";
    const auto executable = executablePath(argv0);
    if (!executable.empty()) {
        const auto directory = executable.parent_path();
        if (isDirectory(directory / "Data")) return directory / "Data";
        if (isDirectory(directory.parent_path() / "Data")) return directory.parent_path() / "Data";
    }
    return error ? std::filesystem::path("Data") : current / "Data";
}

bool standardStreamsAreTerminals() noexcept {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0 && _isatty(_fileno(stdout)) != 0;
#else
    return ::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0;
#endif
}

std::string usage() {
    return "Usage: terminal-music-player [OPTIONS]\n\n"
           "Options:\n"
           "  --data-dir PATH       Use PATH for library.csv, Playlists, and settings.cfg\n"
           "  --non-interactive     Use the deterministic line/stream interface\n"
           "  --no-color            Disable terminal colors\n"
           "  --snapshot            Render one deterministic UI frame and exit\n"
           "  -h, --help            Show this help and exit\n";
}

}  // namespace music_player

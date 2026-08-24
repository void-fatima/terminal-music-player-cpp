#include "Application.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace {

std::filesystem::path findDataDirectory(const char* executable) {
    const auto current = std::filesystem::current_path();
    if (std::filesystem::is_directory(current / "Data")) return current / "Data";
    const auto executableDirectory = std::filesystem::absolute(executable).parent_path();
    if (std::filesystem::is_directory(executableDirectory / "Data")) return executableDirectory / "Data";
    if (std::filesystem::is_directory(executableDirectory.parent_path() / "Data")) {
        return executableDirectory.parent_path() / "Data";
    }
    return current / "Data";
}

}  // namespace

int main(int argc, char* argv[]) {
    auto dataDirectory = findDataDirectory(argv[0]);
    if (argc == 3 && std::string(argv[1]) == "--data-dir") dataDirectory = argv[2];
    else if (argc != 1) {
        std::cerr << "Usage: terminal-music-player [--data-dir PATH]\n";
        return 2;
    }
    music_player::Application application{std::cin, std::cout, std::move(dataDirectory)};
    return application.run();
}

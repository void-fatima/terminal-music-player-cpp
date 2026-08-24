#include "DataLoader.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace music_player {
namespace {

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> parseCsvRow(const std::string& line, bool& valid) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    valid = true;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                field.push_back('"');
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                field.push_back(c);
            }
        } else if (c == ',') {
            fields.push_back(trim(field));
            field.clear();
        } else if (c == '"' && field.empty()) {
            quoted = true;
        } else {
            field.push_back(c);
        }
    }
    if (quoted) valid = false;
    fields.push_back(trim(field));
    return fields;
}

bool parseInt(const std::string& value, int& result) {
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

std::string pathKey(const std::filesystem::path& path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if (error) normalized = path.lexically_normal();
    auto key = normalized.generic_string();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
#endif
    return key;
}

}  // namespace

LoadReport CsvLoader::load(const std::filesystem::path& file, MusicLibrary& library) {
    LoadReport report;
    std::ifstream input(file);
    if (!input) {
        report.warnings.push_back("Cannot open library file: " + file.string());
        return report;
    }

    std::string line;
    std::size_t lineNumber = 0;
    if (std::getline(input, line)) {
        ++lineNumber;
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF
            && static_cast<unsigned char>(line[1]) == 0xBB
            && static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
    }

    Song::Id nextId = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) continue;
        bool valid = false;
        auto fields = parseCsvRow(line, valid);
        if (!valid || fields.size() != 7) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber) + ": expected 7 fields");
            continue;
        }
        int year = 0;
        int seconds = 0;
        if (!parseInt(fields[4], year) || year < 0 || !parseInt(fields[5], seconds) || seconds < 0) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber) + ": invalid year or duration");
            continue;
        }
        if (fields[0].empty() || fields[6].empty()) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber) + ": title and file path are required");
            continue;
        }
        std::filesystem::path audioPath = fields[6];
        if (audioPath.is_relative()) {
            const auto dataRoot = file.parent_path();
            const auto first = audioPath.begin();
            if (first != audioPath.end() && first->string() == dataRoot.filename().string()) {
                audioPath = dataRoot.parent_path() / audioPath;
            } else {
                audioPath = dataRoot / audioPath;
            }
        }
        library.addSong(Song{nextId++, fields[0], fields[1], fields[2], fields[3], audioPath,
                             Song::Duration{seconds * 1000LL}, year});
        ++report.loaded;
    }
    return report;
}

LoadReport M3uLoader::loadDirectory(const std::filesystem::path& directory,
                                    const MusicLibrary& library,
                                    std::vector<Playlist>& playlists) {
    LoadReport report;
    playlists.clear();
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        report.warnings.push_back("Cannot open playlist directory: " + directory.string());
        return report;
    }

    std::unordered_map<std::string, Song::Id> songsByPath;
    for (const auto& song : library.songs()) songsByPath[pathKey(song.filePath())] = song.id();

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.is_regular_file() && entry.path().extension() == ".m3u") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        std::ifstream input(file);
        if (!input) {
            report.warnings.push_back("Cannot read playlist: " + file.string());
            continue;
        }
        std::vector<Song::Id> ids;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            line = trim(line);
            if (line.empty() || line.front() == '#') continue;
            std::filesystem::path audioPath = line;
            if (audioPath.is_relative()) {
                const auto projectRoot = directory.parent_path().parent_path();
                if (audioPath.begin() != audioPath.end()
                    && audioPath.begin()->string() == directory.parent_path().filename().string()) {
                    audioPath = projectRoot / audioPath;
                } else {
                    audioPath = file.parent_path() / audioPath;
                }
            }
            const auto found = songsByPath.find(pathKey(audioPath));
            if (found == songsByPath.end()) {
                report.warnings.push_back(file.filename().string() + " line " + std::to_string(lineNumber)
                                          + ": track is not in the library");
            } else {
                ids.push_back(found->second);
            }
        }
        playlists.emplace_back(file.stem().string(), file, std::move(ids));
        ++report.loaded;
    }
    return report;
}

}  // namespace music_player

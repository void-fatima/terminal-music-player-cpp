#include "DataLoader.h"

#include "PathUtils.h"
#include "StableId.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <ctime>
#include <fstream>
#include <system_error>
#include <unordered_map>

namespace music_player {
namespace {

constexpr std::size_t maxRecordBytes = 1024U * 1024U;
constexpr std::array<const char*, 7> requiredHeader{
    "title", "artist", "album", "genre", "year", "duration_sec", "file_path"};

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

void removeBom(std::string& value) {
    if (value.size() >= 3
        && static_cast<unsigned char>(value[0]) == 0xEF
        && static_cast<unsigned char>(value[1]) == 0xBB
        && static_cast<unsigned char>(value[2]) == 0xBF) {
        value.erase(0, 3);
    }
}

struct CsvRow {
    std::vector<std::string> fields;
    std::string error;
};

CsvRow parseCsvRow(const std::string& line) {
    CsvRow result;
    std::string field;
    bool quoted = false;
    bool fieldWasQuoted = false;
    bool afterQuote = false;

    auto finishField = [&]() {
        result.fields.push_back(fieldWasQuoted ? field : trim(field));
        field.clear();
        fieldWasQuoted = false;
        afterQuote = false;
    };

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (quoted) {
            if (character == '"') {
                if (index + 1 < line.size() && line[index + 1] == '"') {
                    field.push_back('"');
                    ++index;
                } else {
                    quoted = false;
                    afterQuote = true;
                }
            } else {
                field.push_back(character);
            }
            continue;
        }

        if (afterQuote) {
            if (character == ',') {
                finishField();
            } else {
                result.error = "unexpected text after a closing quote";
                return result;
            }
        } else if (character == ',') {
            finishField();
        } else if (character == '"') {
            if (!field.empty()) {
                result.error = "quote in an unquoted field";
                return result;
            }
            quoted = true;
            fieldWasQuoted = true;
        } else {
            field.push_back(character);
        }
    }

    if (quoted) {
        result.error = "unterminated quoted field";
        return result;
    }
    finishField();
    return result;
}

bool parseInt(const std::string& value, int& result) {
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

int currentYear() {
    const std::time_t now = std::time(nullptr);
    std::tm calendar{};
#ifdef _WIN32
    if (localtime_s(&calendar, &now) != 0) return 2100;
#else
    if (localtime_r(&now, &calendar) == nullptr) return 2100;
#endif
    return calendar.tm_year + 1900;
}

std::filesystem::path resolveLibraryPath(const std::filesystem::path& csv,
                                         const std::filesystem::path& supplied) {
    if (supplied.is_absolute()) return supplied.lexically_normal();
    const auto dataRoot = csv.parent_path();
    const auto first = supplied.begin();
    if (first != supplied.end()
        && lowerAscii(first->string()) == lowerAscii(dataRoot.filename().string())) {
        return (dataRoot.parent_path() / supplied).lexically_normal();
    }
    return (dataRoot / supplied).lexically_normal();
}

std::filesystem::path identityPath(const std::filesystem::path& csv,
                                   const std::filesystem::path& resolved,
                                   const std::filesystem::path& supplied) {
    const auto relative = resolved.lexically_relative(csv.parent_path());
    if (!relative.empty() && relative.generic_string().rfind("..", 0) != 0) return relative;
    return supplied.lexically_normal();
}

std::filesystem::path resolvePlaylistPath(const std::filesystem::path& playlist,
                                          const std::filesystem::path& supplied) {
    if (supplied.is_absolute()) return supplied.lexically_normal();
    const auto playlistsDirectory = playlist.parent_path();
    const auto dataRoot = playlistsDirectory.parent_path();
    const auto first = supplied.begin();
    if (first != supplied.end()
        && lowerAscii(first->string()) == lowerAscii(dataRoot.filename().string())) {
        return (dataRoot.parent_path() / supplied).lexically_normal();
    }
    return (playlistsDirectory / supplied).lexically_normal();
}

bool supportedPlaylistExtension(const std::filesystem::path& file) {
    const auto extension = lowerAscii(file.extension().string());
    return extension == ".m3u" || extension == ".m3u8";
}

}  // namespace

LoadReport CsvLoader::load(const std::filesystem::path& file, MusicLibrary& library) {
    LoadReport report;
    std::ifstream input(file, std::ios::binary);
    if (!input) {
        report.warnings.push_back("Cannot open library file: " + file.string());
        return report;
    }

    std::string line;
    if (!std::getline(input, line)) {
        report.warnings.push_back("CSV line 1: required header is missing");
        return report;
    }
    removeBom(line);
    auto header = parseCsvRow(line);
    if (!header.error.empty() || header.fields.size() != requiredHeader.size()) {
        report.warnings.push_back("CSV line 1: invalid header; expected exactly "
                                  "title,artist,album,genre,year,duration_sec,file_path");
        return report;
    }
    for (std::size_t column = 0; column < requiredHeader.size(); ++column) {
        if (header.fields[column] != requiredHeader[column]) {
            report.warnings.push_back("CSV line 1: invalid header column "
                                      + std::to_string(column + 1) + "; expected '"
                                      + requiredHeader[column] + "'");
            return report;
        }
    }

    std::unordered_map<std::string, std::size_t> seenPaths;
    std::unordered_map<Song::Id, std::string> seenIds;
    std::size_t lineNumber = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.size() > maxRecordBytes) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": record exceeds the 1 MiB safety limit");
            continue;
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (trim(line).empty()) continue;

        auto row = parseCsvRow(line);
        if (!row.error.empty()) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber) + ": " + row.error);
            continue;
        }
        if (row.fields.size() != requiredHeader.size()) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": expected exactly 7 fields, got "
                                      + std::to_string(row.fields.size()));
            continue;
        }

        int year = 0;
        int seconds = 0;
        const int maximumYear = currentYear() + 1;
        if (!parseInt(row.fields[4], year) || (year != 0 && (year < 1000 || year > maximumYear))) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": year must be 0 or between 1000 and "
                                      + std::to_string(maximumYear));
            continue;
        }
        if (!parseInt(row.fields[5], seconds) || seconds <= 0 || seconds > 86400) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": duration_sec must be between 1 and 86400");
            continue;
        }
        if (row.fields[0].empty()) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": title is required");
            continue;
        }
        if (row.fields[6].empty()) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": file_path is required");
            continue;
        }

        const std::filesystem::path supplied = pathFromUtf8(row.fields[6]);
        const auto resolved = resolveLibraryPath(file, supplied);
        const auto identity = normalizeIdentityPath(identityPath(file, resolved, supplied));
        if (identity.empty()) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": file_path does not identify a file");
            continue;
        }
        const auto priorPath = seenPaths.find(identity);
        if (priorPath != seenPaths.end()) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": duplicate normalized path from line "
                                      + std::to_string(priorPath->second));
            continue;
        }
        const Song::Id id = stableSongId(identity);
        const auto priorId = seenIds.find(id);
        if (priorId != seenIds.end() && priorId->second != identity) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": stable ID collision with '" + priorId->second + "'");
            continue;
        }

        Song song{id, row.fields[0], row.fields[1], row.fields[2], row.fields[3], resolved,
                  Song::Duration{static_cast<long long>(seconds) * 1000LL}, year};
        if (!library.addSong(std::move(song))) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": duplicate song ID or resolved file path");
            continue;
        }
        seenPaths.emplace(identity, lineNumber);
        seenIds.emplace(id, identity);
        ++report.loaded;

        std::error_code fileError;
        if (!std::filesystem::is_regular_file(resolved, fileError)) {
            report.warnings.push_back("CSV line " + std::to_string(lineNumber)
                                      + ": audio file is missing or unreadable: "
                                      + resolved.string());
        }
    }
    if (input.bad()) report.warnings.push_back("I/O error while reading library file: " + file.string());
    return report;
}

LoadReport M3uLoader::loadDirectory(const std::filesystem::path& directory,
                                    const MusicLibrary& library,
                                    std::vector<Playlist>& playlists) {
    LoadReport report;
    playlists.clear();
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        report.warnings.push_back("Cannot open playlist directory: " + directory.string()
                                  + (error ? " (" + error.message() + ")" : ""));
        return report;
    }

    std::vector<std::filesystem::path> files;
    std::filesystem::directory_iterator iterator(directory, error);
    const std::filesystem::directory_iterator end;
    if (error) {
        report.warnings.push_back("Cannot enumerate playlist directory: " + error.message());
        return report;
    }
    while (iterator != end) {
        std::error_code entryError;
        if (iterator->is_regular_file(entryError) && supportedPlaylistExtension(iterator->path())) {
            files.push_back(iterator->path());
        } else if (entryError) {
            report.warnings.push_back("Cannot inspect playlist entry '"
                                      + iterator->path().filename().string() + "': "
                                      + entryError.message());
        }
        iterator.increment(error);
        if (error) {
            report.warnings.push_back("Error while enumerating playlists: " + error.message());
            error.clear();
        }
    }
    std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
        return lowerAscii(left.filename().string()) < lowerAscii(right.filename().string());
    });

    for (const auto& file : files) {
        std::ifstream input(file, std::ios::binary);
        if (!input) {
            report.warnings.push_back("Cannot read playlist: " + file.string());
            continue;
        }
        std::vector<Song::Id> ids;
        std::string line;
        std::size_t lineNumber = 0;
        bool rejected = false;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (lineNumber == 1) removeBom(line);
            if (line.size() > maxRecordBytes) {
                report.warnings.push_back(file.filename().string() + " line "
                                          + std::to_string(lineNumber)
                                          + ": entry exceeds the 1 MiB safety limit");
                rejected = true;
                continue;
            }
            line = trim(line);
            if (line.empty() || line.front() == '#') continue;
            const auto resolved = resolvePlaylistPath(file, pathFromUtf8(line));
            const auto song = library.findByPath(resolved);
            if (!song) {
                report.warnings.push_back(file.filename().string() + " line "
                                          + std::to_string(lineNumber)
                                          + ": track is not in the library: " + line);
            } else {
                ids.push_back(song->get().id());
            }
        }
        if (input.bad()) {
            report.warnings.push_back("I/O error while reading playlist: " + file.string());
            continue;
        }
        if (ids.empty()) {
            report.warnings.push_back(file.filename().string()
                                      + ": playlist is empty after resolving library tracks"
                                      + (rejected ? " (one or more entries were rejected)" : ""));
        }
        playlists.emplace_back(file.stem().string(), file, std::move(ids));
        ++report.loaded;
    }
    return report;
}

}  // namespace music_player

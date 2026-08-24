#include "StableId.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace music_player {
namespace {

std::string foldAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z'
            ? static_cast<char>(character - 'A' + 'a')
            : static_cast<char>(character);
    });
    return value;
}

}  // namespace

std::string normalizeIdentityPath(const std::filesystem::path& path) {
    auto normalized = path.lexically_normal().generic_string();
    while (normalized.rfind("./", 0) == 0) normalized.erase(0, 2);
    while (!normalized.empty() && normalized.front() == '/') normalized.erase(0, 1);
    return foldAscii(std::move(normalized));
}

Song::Id stableSongId(const std::filesystem::path& identityPath) noexcept {
    // FNV-1a 64 is explicitly specified here rather than relying on
    // implementation-defined std::hash output.
    constexpr Song::Id offset = 14695981039346656037ULL;
    constexpr Song::Id prime = 1099511628211ULL;
    Song::Id hash = offset;
    const auto bytes = normalizeIdentityPath(identityPath);
    for (const unsigned char byte : bytes) {
        hash ^= static_cast<Song::Id>(byte);
        hash *= prime;
    }
    return hash;
}

std::string normalizedPathKey(const std::filesystem::path& path) {
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if (error) normalized = path.lexically_normal();
    return foldAscii(normalized.generic_string());
}

}  // namespace music_player

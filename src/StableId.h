#pragma once

#include "Song.h"

#include <filesystem>
#include <string>

namespace music_player {

// Identity paths use '/' separators, lexical dot-segment removal, and ASCII
// case folding. The rule is deliberately platform-independent.
std::string normalizeIdentityPath(const std::filesystem::path& path);
Song::Id stableSongId(const std::filesystem::path& identityPath) noexcept;

// Runtime lookup keys additionally normalize an absolute/resolved path.
std::string normalizedPathKey(const std::filesystem::path& path);

}  // namespace music_player

#pragma once

#include <filesystem>
#include <string_view>

namespace music_player {

std::filesystem::path pathFromUtf8(std::string_view value);

}  // namespace music_player

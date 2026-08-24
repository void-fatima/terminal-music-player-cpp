#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace music_player::utf8 {

std::size_t displayWidth(std::string_view text) noexcept;
std::string truncate(std::string_view text, std::size_t columns);
std::string padRight(std::string_view text, std::size_t columns);

}  // namespace music_player::utf8

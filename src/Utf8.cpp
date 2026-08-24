#include "Utf8.h"

#include <algorithm>
#include <cstdint>

namespace music_player::utf8 {
namespace {

struct Decoded {
    std::uint32_t codepoint{0xFFFD};
    std::size_t bytes{1};
    bool valid{false};
};

Decoded decode(std::string_view text, std::size_t offset) noexcept {
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80) return {first, 1, true};
    std::size_t count = 0;
    std::uint32_t codepoint = 0;
    if ((first & 0xE0U) == 0xC0U) { count = 2; codepoint = first & 0x1FU; }
    else if ((first & 0xF0U) == 0xE0U) { count = 3; codepoint = first & 0x0FU; }
    else if ((first & 0xF8U) == 0xF0U) { count = 4; codepoint = first & 0x07U; }
    else return {};
    if (offset + count > text.size()) return {};
    for (std::size_t index = 1; index < count; ++index) {
        const auto continuation = static_cast<unsigned char>(text[offset + index]);
        if ((continuation & 0xC0U) != 0x80U) return {};
        codepoint = (codepoint << 6U) | (continuation & 0x3FU);
    }
    const bool overlong = (count == 2 && codepoint < 0x80)
        || (count == 3 && codepoint < 0x800) || (count == 4 && codepoint < 0x10000);
    if (overlong || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return {};
    return {codepoint, count, true};
}

bool inRange(std::uint32_t value, std::uint32_t first, std::uint32_t last) noexcept {
    return value >= first && value <= last;
}

std::size_t codepointWidth(std::uint32_t value) noexcept {
    if (value == 0 || value < 0x20 || inRange(value, 0x7F, 0x9F)) return 0;
    if (inRange(value, 0x0300, 0x036F) || inRange(value, 0x0483, 0x0489)
        || inRange(value, 0x0591, 0x05BD) || inRange(value, 0x0610, 0x061A)
        || inRange(value, 0x064B, 0x065F) || inRange(value, 0x0670, 0x0670)
        || inRange(value, 0x06D6, 0x06ED) || inRange(value, 0x1AB0, 0x1AFF)
        || inRange(value, 0x1DC0, 0x1DFF) || inRange(value, 0x20D0, 0x20FF)
        || inRange(value, 0xFE00, 0xFE0F) || inRange(value, 0xFE20, 0xFE2F)
        || inRange(value, 0xE0100, 0xE01EF)) return 0;
    if (inRange(value, 0x1100, 0x115F) || value == 0x2329 || value == 0x232A
        || inRange(value, 0x2E80, 0xA4CF) || inRange(value, 0xAC00, 0xD7A3)
        || inRange(value, 0xF900, 0xFAFF) || inRange(value, 0xFE10, 0xFE19)
        || inRange(value, 0xFE30, 0xFE6F) || inRange(value, 0xFF00, 0xFF60)
        || inRange(value, 0xFFE0, 0xFFE6) || inRange(value, 0x1F300, 0x1FAFF)
        || inRange(value, 0x20000, 0x3FFFD)) return 2;
    return 1;
}

}  // namespace

std::size_t displayWidth(std::string_view text) noexcept {
    std::size_t width = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto decoded = decode(text, offset);
        width += decoded.valid ? codepointWidth(decoded.codepoint) : 1;
        offset += decoded.bytes;
    }
    return width;
}

std::string truncate(std::string_view text, std::size_t columns) {
    if (displayWidth(text) <= columns) return std::string(text);
    if (columns == 0) return {};
    const std::string suffix = columns >= 3 ? "..." : std::string(columns, '.');
    const std::size_t available = columns - suffix.size();
    std::string result;
    std::size_t used = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto decoded = decode(text, offset);
        const auto width = decoded.valid ? codepointWidth(decoded.codepoint) : 1;
        if (used + width > available) break;
        if (decoded.valid) result.append(text.substr(offset, decoded.bytes));
        else result.push_back('?');
        used += width;
        offset += decoded.bytes;
    }
    result += suffix;
    return result;
}

std::string padRight(std::string_view text, std::size_t columns) {
    auto result = truncate(text, columns);
    const auto width = displayWidth(result);
    if (width < columns) result.append(columns - width, ' ');
    return result;
}

}  // namespace music_player::utf8

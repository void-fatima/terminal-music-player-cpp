#include "PathUtils.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <string>

namespace music_player {

std::filesystem::path pathFromUtf8(std::string_view value) {
#ifdef _WIN32
    if (value.empty()) return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) return std::filesystem::path(std::string(value));
    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), wide.data(), required) <= 0) {
        return std::filesystem::path(std::string(value));
    }
    return std::filesystem::path(wide);
#else
    return std::filesystem::path(std::string(value));
#endif
}

}  // namespace music_player

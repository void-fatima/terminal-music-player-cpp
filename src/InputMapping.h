#pragma once

#include <string_view>

namespace music_player {

enum class UiCommand {
    None,
    Activate,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    FocusNext,
    PlayPause,
    Stop,
    Next,
    Previous,
    SeekBackward,
    SeekForward,
    VolumeDown,
    VolumeUp,
    ChangeMode,
    Search,
    Help,
    Quit
};

UiCommand mapKeyName(std::string_view key) noexcept;

}  // namespace music_player

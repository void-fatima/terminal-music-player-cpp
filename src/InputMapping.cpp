#include "InputMapping.h"

namespace music_player {

UiCommand mapKeyName(std::string_view key) noexcept {
    if (key == "enter") return UiCommand::Activate;
    if (key == "up" || key == "k") return UiCommand::MoveUp;
    if (key == "down" || key == "j") return UiCommand::MoveDown;
    if (key == "left") return UiCommand::MoveLeft;
    if (key == "right") return UiCommand::MoveRight;
    if (key == "tab") return UiCommand::FocusNext;
    if (key == "space") return UiCommand::PlayPause;
    if (key == "s") return UiCommand::Stop;
    if (key == "n") return UiCommand::Next;
    if (key == "p") return UiCommand::Previous;
    if (key == "h") return UiCommand::SeekBackward;
    if (key == "l") return UiCommand::SeekForward;
    if (key == "-" || key == "_") return UiCommand::VolumeDown;
    if (key == "+" || key == "=") return UiCommand::VolumeUp;
    if (key == "m") return UiCommand::ChangeMode;
    if (key == "/") return UiCommand::Search;
    if (key == "?" || key == "f1") return UiCommand::Help;
    if (key == "q") return UiCommand::Quit;
    return UiCommand::None;
}

}  // namespace music_player

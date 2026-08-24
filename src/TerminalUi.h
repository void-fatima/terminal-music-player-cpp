#pragma once

#include "SessionController.h"

#include <string>

namespace music_player {

class TerminalUi {
public:
    explicit TerminalUi(SessionController& session);
    int run();
    std::string snapshot(int width = 150, int height = 42);

private:
    SessionController& session_;
};

}  // namespace music_player

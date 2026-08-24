#pragma once

#include "SessionController.h"

namespace music_player {

class TerminalUi {
public:
    explicit TerminalUi(SessionController& session);
    int run();

private:
    SessionController& session_;
};

}  // namespace music_player

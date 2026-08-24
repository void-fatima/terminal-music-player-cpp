#pragma once

#include "IAudioBackend.h"

#include <filesystem>
#include <iosfwd>
#include <memory>

namespace music_player {

// Compatibility wrapper for deterministic stream-driven sessions. Interactive
// terminals use TerminalUi directly through main().
class Application {
public:
    Application(std::istream& input,
                std::ostream& output,
                std::filesystem::path dataDirectory = "Data",
                std::unique_ptr<IAudioBackend> audioBackend = {});
    int run();

private:
    std::istream& input_;
    std::ostream& output_;
    std::filesystem::path dataDirectory_;
    std::unique_ptr<IAudioBackend> audioBackend_;
};

}  // namespace music_player

#include "Application.h"

#include "SessionController.h"
#include "StreamUi.h"

#include <utility>

namespace music_player {

Application::Application(std::istream& input,
                         std::ostream& output,
                         std::filesystem::path dataDirectory,
                         std::unique_ptr<IAudioBackend> audioBackend)
    : input_(input), output_(output), dataDirectory_(std::move(dataDirectory)),
      audioBackend_(std::move(audioBackend)) {}

int Application::run() {
    SessionController session{dataDirectory_, std::move(audioBackend_)};
    if (!session.load()) return 1;
    StreamUi ui{session, input_, output_};
    const int result = ui.run();
    return session.shutdown() ? result : 1;
}

}  // namespace music_player

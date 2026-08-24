#include "Application.h"
#include "CliOptions.h"
#include "SessionController.h"
#include "TerminalUi.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> arguments;
        arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);
        for (int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
        const auto parsed = music_player::parseCli(arguments);
        if (!parsed) {
            std::cerr << "Error: " << parsed.error << "\n\n" << music_player::usage();
            return 2;
        }
        if (parsed.options.help) {
            std::cout << music_player::usage();
            return 0;
        }
#ifdef _WIN32
        if (parsed.options.noColor) (void)_putenv_s("NO_COLOR", "1");
#else
        if (parsed.options.noColor) (void)setenv("NO_COLOR", "1", 1);
#endif
        const auto dataDirectory = music_player::resolveDataDirectory(parsed.options, argv[0]);
        if (parsed.options.nonInteractive || !music_player::standardStreamsAreTerminals()) {
            music_player::Application application{std::cin, std::cout, dataDirectory};
            return application.run();
        }

        music_player::SessionController session{dataDirectory};
        if (!session.load()) {
            std::cerr << "Error: cannot load data from " << dataDirectory.string() << '\n';
            return 1;
        }
        music_player::TerminalUi interface{session};
        const int result = interface.run();
        if (!session.shutdown()) {
            std::cerr << "Error: " << session.message() << '\n';
            return 1;
        }
        return result;
    } catch (const std::filesystem::filesystem_error& error) {
        std::cerr << "Filesystem error: " << error.what() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Unexpected error: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "Unexpected non-standard failure.\n";
    }
    return 1;
}

#pragma once

#include "SessionController.h"

#include <iosfwd>
#include <optional>
#include <string>

namespace music_player {

class StreamUi {
public:
    StreamUi(SessionController& session, std::istream& input, std::ostream& output);
    int run();

private:
    bool execute(const std::string& line);
    void help() const;
    void listLibrary() const;
    void listQueue() const;
    void listPlaylists() const;
    void status() const;
    void showResult(bool success);

    static std::string trim(std::string value);
    static std::string lower(std::string value);
    static std::optional<std::size_t> index(std::string value, std::size_t count);

    SessionController& session_;
    std::istream& input_;
    std::ostream& output_;
};

}  // namespace music_player

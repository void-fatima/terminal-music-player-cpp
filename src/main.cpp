#include "Application.h"

#include <iostream>

int main() {
    music_player::Application application{std::cin, std::cout};
    return application.run();
}

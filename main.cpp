#define SDL_MAIN_HANDLED
#include "Application.hpp"

#include <exception>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    try {
        elysium::Application app;
        if (!app.initialize(argc, argv)) {
            return 1;
        }
        app.run();
        app.shutdown();
        return 0;
    } catch (const std::exception& e) {
        spdlog::critical("Unhandled exception: {}", e.what());
        return 1;
    }
}

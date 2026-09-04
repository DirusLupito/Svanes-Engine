#pragma once

#include <svanes/registry.hpp>

#include <cstdint>
#include <string>

namespace svanes {

class IGame;

/**
 * Defines the settings for the application.
 * 
 * Currently this only includes the title and dimensions of the game window.
 *
 * FIELDS:
 * - title: The title displayed by the application window.
 * - width: The initial width of the application window.
 * - height: The initial height of the application window.
 */
struct ApplicationSettings {
    std::string title = "Svanes Engine";
    int32_t width = 1920;
    int32_t height = 1080;
};

/**
 * Represents the main application that runs the game loop and manages
 * the SDL window and renderer. It is responsible for initializing SDL,
 * creating the window and renderer, and running the game loop until the
 * game indicates that it should quit.
 */
class Application final {
public:
    /**
     * Constructs an Application with the specified settings.
     * @param settings The settings for the application, including the window title and dimensions.
     */
    explicit Application(ApplicationSettings settings = {});

    /**
     * Runs the application with the provided game instance. This method initializes SDL,
     * creates the window and renderer, and enters the game loop until the game indicates
     * that it should quit. It returns an integer status code, where 0 indicates success
     * and any non-zero value indicates an error.
     * 
     * @param game The game instance to run within the application.
     * @return An integer status code indicating the result of the application run.
     */
    int32_t run(IGame& game);

private:
    // The settings for the application, including the window title and dimensions.
    ApplicationSettings settings;

    // The registry that holds all entities and their components in the game world.
    Registry world;
};

} // namespace svanes

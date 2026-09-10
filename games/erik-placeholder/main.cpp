#include "erik_game.hpp"

#include <svanes/application.hpp>

// TASK 1, running the engine: the game is handed to a svanes::Application, which
// opens the window and runs the engine's loop until the game asks to quit
int32_t main()
{
    svanes::Application application({
        .title = "Erik's Game",
        .width = 1920,
        .height = 1080,
    });

    ErikGame game;
    return application.run(game);
}

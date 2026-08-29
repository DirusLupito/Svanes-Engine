#include "chris_game.hpp"

#include <svanes/application.hpp>

int main()
{
    const svanes::Application application({
        .title = "Chris's Game",
        .width = 1920,
        .height = 1080,
    });

    ChrisGame game;
    return application.run(game);
}

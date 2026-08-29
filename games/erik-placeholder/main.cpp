#include "erik_game.hpp"

#include <svanes/application.hpp>

int main()
{
    const svanes::Application application({
        .title = "Erik's Game",
        .width = 1920,
        .height = 1080,
    });

    ErikGame game;
    return application.run(game);
}

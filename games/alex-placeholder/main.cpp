#include "alex_game.hpp"

#include <svanes/application.hpp>

int main()
{
    const svanes::Application application({
        .title = "Alex's Game",
        .width = 1920,
        .height = 1080,
    });

    AlexGame game;
    return application.run(game);
}

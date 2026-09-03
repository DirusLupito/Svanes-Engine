#include "abm_game.hpp"

#include <svanes/application.hpp>

int32_t main()
{
    const svanes::Application application({
        .title = "Abm's Game",
        .width = 1920,
        .height = 1080,
    });

    AbmGame game;
    return application.run(game);
}

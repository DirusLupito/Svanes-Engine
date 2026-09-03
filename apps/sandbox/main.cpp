#include "sandbox_game.hpp"

#include <svanes/application.hpp>

int32_t main()
{
    const svanes::Application application({
        .title = "Svanes Engine Poopbox",
        .width = 1920,
        .height = 1080,
    });

    SandboxGame game;
    return application.run(game);
}

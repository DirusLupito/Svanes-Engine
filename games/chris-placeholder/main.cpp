#include "chris_game.hpp"

#include <svanes/application.hpp>

#include <string>

int32_t main(int32_t argc, char** argv)
{
    const std::string server_host = argc > 1 ? argv[1] : kChrisDefaultServerHost;

    svanes::Application application({
        .title = "Chris's Game",
        .width = 1920,
        .height = 1080,
    });

    ChrisGame game(server_host);
    return application.run(game);
}

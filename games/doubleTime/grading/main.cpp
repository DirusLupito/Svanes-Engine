#include "double_time_game.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <stdexcept>
#include <string>

int32_t main(int32_t argc, char **argv) {
    if (argc > 2) {
        throw std::invalid_argument(
            "Usage: svanes_game_double_time_grading [server-host]");
    }
    const std::string server_host = argc > 1 ? argv[1] : kDoubleTimeDefaultServerHost;

    svanes::NetworkClient client({
        .title = "Double Time",
        .width = 1920,
        .height = 1080,
    });

    DoubleTimeGame game(server_host);
    return client.run(game);
}

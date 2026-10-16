#include "double_time_game.hpp"

#include <svanes/application.hpp>

#include <stdexcept>
#include <charconv>
#include <string>

int32_t main(int32_t argc, char **argv) {
    if (argc > 4) {
        throw std::invalid_argument(
            "Usage: svanes_game_double_time [server-host] [bridge-port] [platform]");
    }
    const std::string server_host = argc > 1 ? argv[1] : kDoubleTimeDefaultServerHost;
    std::uint16_t bridge_port = 0;
    if (argc > 2) {
        unsigned int parsed_port = 0;
        const std::string port_text = argv[2];
        const auto result = std::from_chars(
            port_text.data(), port_text.data() + port_text.size(), parsed_port);
        if (result.ec != std::errc{} || result.ptr != port_text.data() +
                                                  port_text.size() ||
            parsed_port == 0 || parsed_port > 65535) {
            throw std::invalid_argument("The bridge port must be between 1 and 65535.");
        }
        bridge_port = static_cast<std::uint16_t>(parsed_port);
    }
    const bool platform_role = argc > 3 && std::string{argv[3]} == "platform";
    if (argc > 3 && !platform_role && std::string{argv[3]} != "character") {
        throw std::invalid_argument("The bridge role must be 'character' or 'platform'.");
    }

    svanes::Application application({
        .title = "Double Time",
        .width = 1920,
        .height = 1080,
    });

    DoubleTimeGame game(server_host, bridge_port, platform_role);
    return application.run(game);
}

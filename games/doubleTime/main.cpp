#include "chris_game.hpp"
#include "p2p_game.hpp"

#include <svanes/application.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace {

ClientRole ParseRole(std::string_view role_text) {
    if (role_text == "character") {
        return ClientRole::Character;
    }
    if (role_text == "platform") {
        return ClientRole::Platform;
    }
    if (role_text == "spectator") {
        return ClientRole::Spectator;
    }
    throw std::invalid_argument(
        "Unrecognized client role \"" + std::string{role_text} +
        "\"; expected \"character\", \"platform\", or \"spectator\"."
    );
}

} // namespace

int32_t main(int32_t argc, char **argv) {
    if (argc > 1 && std::string_view{argv[1]} == "p2p") {
        if (argc > 4) throw std::invalid_argument("P2P expects an optional port and entry host.");
        const auto port = argc > 2 ? static_cast<std::uint16_t>(std::stoul(argv[2])) : kDoubleTimeP2PHostPort;
        std::optional<svanes::UdpAddress> entry;
        if (argc > 3) entry = svanes::UdpAddress{argv[3], kDoubleTimeP2PHostPort};
        svanes::Application application({.title = "Double Time P2P", .width = 640, .height = 360});
        DoubleTimeP2PGame game(port, entry);
        return application.run(game);
    }
    if (argc > 3) {
        throw std::invalid_argument(
            "Expected at most a server host and client role.");
    }
    const std::string server_host = argc > 1 ? argv[1] : kChrisDefaultServerHost;
    const ClientRole role = argc > 2 ? ParseRole(argv[2]) : ClientRole::Character;

    svanes::Application application({
        .title = "Double Time",
        .width = 1920,
        .height = 1080,
    });

    ChrisGame game(server_host, role);
    return application.run(game);
}

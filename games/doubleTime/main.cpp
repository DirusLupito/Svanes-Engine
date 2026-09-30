#include "chris_game.hpp"

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

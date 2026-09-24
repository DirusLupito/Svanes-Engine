#include "erik_game.hpp"

#include <svanes/application.hpp>

#include <charconv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

/**
 * Reads an unsigned command-line number without accepting a partial match.
 * @param text The argument text.
 * @return The parsed value.
 * @throws std::invalid_argument if the argument is not a representable unsigned integer.
 */
std::uint64_t ReadNumber(std::string_view text)
{
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::invalid_argument("Expected an unsigned integer: " + std::string{text});
    }
    return value;
}

}

// TASK 1, running the engine: the game is handed to a svanes::Application, which
// opens the window and runs the engine's loop until the game asks to quit
int32_t main(int32_t argc, char** argv)
{
    try {
        std::uint32_t peer = 0;
        std::uint32_t players = 3;
        std::uint16_t port_base = 45000;
        svanes::SessionId session = 1;
        for (int32_t index = 1; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--help") {
                std::cout << "Usage: svanes_game_erik [--peer N [--players N] [--port-base N] [--session N]]\n"
                    << "No options starts single-player. Multiplayer defaults to 3 players on loopback.\n"
                    << "Open every configured window, then press Enter in each to mark it ready.\n";
                return 0;
            }
            if (option != "--peer" && option != "--players" &&
                option != "--port-base" && option != "--session") {
                throw std::invalid_argument("Unknown option: " + std::string{option});
            }
            if (++index >= argc) {
                throw std::invalid_argument("Missing value for " + std::string{option});
            }
            const auto value = ReadNumber(argv[index]);
            if (option == "--session") {
                session = value;
            } else if (option == "--port-base") {
                if (value > std::numeric_limits<std::uint16_t>::max()) {
                    throw std::invalid_argument("Port base must fit in 16 bits.");
                }
                port_base = static_cast<std::uint16_t>(value);
            } else {
                if (value > std::numeric_limits<std::uint32_t>::max()) {
                    throw std::invalid_argument("Peer id and player count must fit in 32 bits.");
                }
                if (option == "--peer") {
                    peer = static_cast<std::uint32_t>(value);
                } else {
                    players = static_cast<std::uint32_t>(value);
                }
            }
        }

        std::optional<GooseNetworkConfiguration> configuration;
        if (argc > 1) {
            configuration = MakeLoopbackGooseConfiguration({peer}, players, port_base, session);
        }
        svanes::Application application({
            .title = configuration
                ? "Erik's Game - Peer " + std::to_string(peer) + "/" + std::to_string(players)
                : "Erik's Game",
            .width = 640,
            .height = 900,
        });

        ErikGame game(std::move(configuration));
        return application.run(game);
    } catch (const std::exception& error) {
        std::cerr << "Erik's Game: " << error.what() << '\n';
        return 1;
    }
}

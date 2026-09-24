#include "erik_game.hpp"

#include <svanes/application.hpp>

#include <charconv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

/**
 * Reads one roster member from ID@IPv4:PORT.
 * @param text The command-line endpoint.
 * @return The peer identity and address, with IPv4 validation left to the UDP pipe.
 * @throws std::invalid_argument for missing fields or out-of-range ids and ports.
 */
GoosePeerEndpoint ReadEndpoint(std::string_view text)
{
    const auto at = text.find('@');
    const auto colon = text.find(':');
    if (at == std::string_view::npos || colon == std::string_view::npos ||
        at == 0 || colon <= at + 1 || colon + 1 >= text.size()) {
        throw std::invalid_argument("Expected an endpoint in ID@IPv4:PORT form: " + std::string{text});
    }
    const auto id = ReadNumber(text.substr(0, at));
    const auto port = ReadNumber(text.substr(colon + 1));
    if (id == 0 || id > std::numeric_limits<std::uint32_t>::max() ||
        port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("Endpoint requires a nonzero 32-bit peer id and a port from 1 to 65535.");
    }
    return {{static_cast<std::uint32_t>(id)}, std::string{text.substr(at + 1, colon - at - 1)},
        static_cast<std::uint16_t>(port)};
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
        std::vector<GoosePeerEndpoint> endpoints;
        bool loopback_options = false;
        for (int32_t index = 1; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--help") {
                std::cout << "Usage: svanes_game_erik [--peer N [--players N] [--port-base N] [--session N]]\n"
                    << "LAN: svanes_game_erik --peer N --endpoint ID@IPv4:PORT --endpoint ID@IPv4:PORT ... [--session N]\n"
                    << "No options starts single-player. Multiplayer defaults to 3 players on loopback.\n"
                    << "Repeat --endpoint for every player, including yourself. Use the same roster on every computer.\n"
                    << "Explicit endpoints determine the player count and cannot be combined with --players or --port-base.\n"
                    << "Open every configured window, then press Enter in each to mark it ready.\n";
                return 0;
            }
            if (option != "--peer" && option != "--players" &&
                option != "--port-base" && option != "--session" && option != "--endpoint") {
                throw std::invalid_argument("Unknown option: " + std::string{option});
            }
            if (++index >= argc) {
                throw std::invalid_argument("Missing value for " + std::string{option});
            }
            if (option == "--endpoint") {
                endpoints.push_back(ReadEndpoint(argv[index]));
                continue;
            }
            if (option == "--players" || option == "--port-base") {
                loopback_options = true;
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
            if (endpoints.empty()) {
                configuration = MakeLoopbackGooseConfiguration({peer}, players, port_base, session);
            } else {
                if (loopback_options) {
                    throw std::invalid_argument("Use --endpoint or --players/--port-base, not both.");
                }
                configuration = GooseNetworkConfiguration{session, {peer}, std::move(endpoints)};
            }
            std::cout << "Session " << configuration->session << ", local peer " << peer << '\n';
            for (const auto& endpoint : configuration->peers) {
                std::cout << "  Peer " << endpoint.peer.value << " at " << endpoint.host << ':' << endpoint.port
                    << (endpoint.peer.value == peer ? " (local)" : "") << '\n';
            }
        }
        svanes::Application application({
            .title = configuration
                ? "Erik's Game - Peer " + std::to_string(peer) + " (" + std::to_string(configuration->peers.size()) + " players)"
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

#include "erik_game.hpp"

#include <svanes/application.hpp>

#include <charconv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <optional>
#include <utility>

namespace {

/**
 * Reads a UDP port without accepting a partial match.
 * @param text The port text.
 * @return The port.
 * @throws std::invalid_argument unless the text is a number from 1 to 65535.
 */
std::uint16_t ReadPort(std::string_view text)
{
    std::uint32_t port = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), port);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("Expected a port from 1 to 65535: " + std::string{text});
    }
    return static_cast<std::uint16_t>(port);
}

/**
 * Reads an address in IPv4:PORT form.
 * @param text The command-line address.
 * @return The host and port, with IPv4 validation left to the UDP pipe.
 * @throws std::invalid_argument for a missing host or an invalid port.
 */
svanes::UdpAddress ReadAddress(std::string_view text)
{
    const auto colon = text.rfind(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 >= text.size()) {
        throw std::invalid_argument("Expected an address in IPv4:PORT form: " + std::string{text});
    }
    return {std::string{text.substr(0, colon)}, ReadPort(text.substr(colon + 1))};
}

}

// TASK 1, running the engine: the game is handed to a svanes::Application, which
// opens the window and runs the engine's loop until the game asks to quit
int32_t main(int32_t argc, char** argv)
{
    try {
        std::optional<svanes::UdpAddress> join_address;
        std::optional<std::uint16_t> port;
        for (int32_t index = 1; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--help") {
                std::cout << "Usage: svanes_game_erik [--join IPv4:PORT] [--port N]\n"
                    << "No options starts a new world that others can join on port 45000.\n"
                    << "--join connects to any player already in a world, listening on port 45001.\n"
                    << "--port chooses the local port. Every copy on one computer needs its own.\n";
                return 0;
            }
            if (option == "--port") {
                if (++index >= argc || port) {
                    throw std::invalid_argument("--port requires one value and may appear once.");
                }
                port = ReadPort(argv[index]);
                continue;
            }
            if (option != "--join") {
                throw std::invalid_argument("Unknown option: " + std::string{option});
            }
            if (++index >= argc || join_address) {
                throw std::invalid_argument("--join requires one address and may appear once.");
            }
            join_address = ReadAddress(argv[index]);
        }
        svanes::Application application({
            .title = join_address
                ? "Erik's Game - joining " + join_address->host + ":" + std::to_string(join_address->port)
                : "Erik's Game",
            .width = 640,
            .height = 900,
        });

        ErikGame game(port.value_or(join_address ? GooseJoinPort : GooseHostPort), std::move(join_address));
        return application.run(game);
    } catch (const std::exception& error) {
        std::cerr << "Erik's Game: " << error.what() << '\n';
        return 1;
    }
}

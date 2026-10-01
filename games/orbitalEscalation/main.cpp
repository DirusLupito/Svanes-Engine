#include "orbital_escalation_game.hpp"

#include <svanes/application.hpp>

#include <charconv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

/**
 * Reads a port from a command-line argument.
 *
 * @param text The decimal port number.
 * @return A port from 1 to 65535.
 * @throws std::invalid_argument if text is not a complete decimal number in
 * the permitted range.
 */
static std::uint16_t ReadPort(std::string_view text) {
    std::uint32_t port = 0;
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        port == 0 || port > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("Expected a port from 1 to 65535: " +
                                    std::string{text});
    }

    return static_cast<std::uint16_t>(port);
}

/**
 * Reads the peer address supplied when joining a game.
 *
 * @param text The host and port separated by a colon.
 * @return The destination address for the join request.
 * @throws std::invalid_argument if the host or separator is missing, or the
 * port is invalid.
 */
static svanes::UdpAddress ReadAddress(std::string_view text) {
    const auto colon = text.rfind(':');
    if (colon == std::string_view::npos || colon == 0) {
        throw std::invalid_argument("Expected an address in IPv4:PORT form: " +
                                    std::string{text});
    }
    return {std::string{text.substr(0, colon)},
            ReadPort(text.substr(colon + 1))};
}

/**
 * Runs a new world or joins an existing game using command-line options.
 *
 * @param argc The argument count, including the program name.
 * @param argv The program name and command-line arguments.
 * @return Zero for help, the application's exit status after play, or 1 if
 * startup or gameplay reports an exception.
 */
int32_t main(int32_t argc, char **argv) {
    try {
        std::optional<svanes::UdpAddress> join_address;
        std::optional<svanes::UdpAddress> server_address;
        std::optional<std::uint16_t> port;
        for (int32_t index = 1; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--help") {
                std::cout << "Usage: svanes_game_orbital_escalation [--join "
                             "IPv4:PORT] [--port N]\n"
                          << "No options starts a world on UDP port 45000.\n"
                          << "Joining listens on port 45001 unless --port is "
                             "supplied.\n"
                          << "Every copy on one computer needs its own port.\n";
                std::cout
                    << "--server HOST:PORT connects to a headless server; "
                       "choose TCP port 45010, 45011, or 45012.\n";
                return 0;
            }

            if (option == "--server") {
                if (++index >= argc || server_address) {
                    throw std::invalid_argument(
                        "--server requires one address and may appear once.");
                }
                server_address = ReadAddress(argv[index]);
            } else if (option == "--port") {
                if (++index >= argc || port) {
                    throw std::invalid_argument(
                        "--port requires one value and may appear once.");
                }
                port = ReadPort(argv[index]);
            } else if (option == "--join") {
                if (++index >= argc || join_address) {
                    throw std::invalid_argument(
                        "--join requires one address and may appear once.");
                }
                join_address = ReadAddress(argv[index]);
            } else {
                throw std::invalid_argument("Unknown option: " +
                                            std::string{option});
            }
        }

        if (server_address && (join_address || port)) {
            throw std::invalid_argument(
                "--server cannot be combined with --join or --port.");
        }
        svanes::Application application({
            .title = join_address ? "Orbital Escalation - joining " +
                                        join_address->host + ":" +
                                        std::to_string(join_address->port)
                                  : "Orbital Escalation",
            .width = 1920,
            .height = 1080,
        });

        if (server_address) {
            OrbitalEscalationGame game(server_address->host,
                                       server_address->port);
            return application.run(game);
        }
        OrbitalEscalationGame game(
            port.value_or(join_address ? OrbitalJoinPort : OrbitalHostPort),
            std::move(join_address));

        return application.run(game);
    } catch (const std::exception &error) {
        std::cerr << "Orbital Escalation: " << error.what() << '\n';
        return 1;
    }
}

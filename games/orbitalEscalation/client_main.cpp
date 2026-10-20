#include "orbital_escalation_client.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <charconv>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

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
 * Reads an endpoint address supplied through the command line.
 *
 * @param text The host and port separated by a colon.
 * @return The address with a validated port.
 * @throws std::invalid_argument if the host or separator is missing, or the
 * port is invalid.
 */
static std::string ReadAddress(std::string_view text) {
    const auto colon = text.rfind(':');
    if (colon == std::string_view::npos || colon == 0) {
        throw std::invalid_argument("Expected an address in IPv4:PORT form: " +
                                    std::string{text});
    }
    return std::string{text.substr(0, colon)} + ':' +
           std::to_string(ReadPort(text.substr(colon + 1)));
}

/**
 * Runs the Orbital Escalation client using command-line options.
 *
 * @param argc The argument count, including the program name.
 * @param argv The program name and command-line arguments.
 * @return Zero for help, the application's exit status after play, or 1 if
 * startup or gameplay reports an exception.
 */
int32_t main(int32_t argc, char **argv) {
    try {
        std::optional<std::string> server_address;
        for (int32_t index = 1; index < argc; ++index) {
            const std::string_view option{argv[index]};
            if (option == "--help") {
                std::cout << "Usage: svanes_game_orbital_escalation --server "
                             "IPv4:PORT\n"
                          << "--server: The server's address for admission and "
                             "gameplay messages.\n";
                return 0;
            }

            if (option == "--server") {
                if (++index >= argc || server_address) {
                    throw std::invalid_argument(
                        "--server requires one address and may appear once.");
                }
                server_address = ReadAddress(argv[index]);
            } else {
                throw std::invalid_argument("Unknown option: " +
                                            std::string{option});
            }
        }

        if (!server_address) {
            throw std::invalid_argument("--server address is required.");
        }
        svanes::NetworkClient network({
            .title = "Orbital Escalation",
            .width = 1920,
            .height = 1080,
        });

        OrbitalEscalationClient client(std::move(*server_address));

        return network.run(client);
    } catch (const std::exception &error) {
        std::cerr << "Orbital Escalation: " << error.what() << '\n';
        return 1;
    }
}

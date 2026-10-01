#include "OrbitalEscalationServer.hpp"
#include <charconv>
#include <iostream>
#include <limits>
#include <string_view>
#include <svanes/asset_path.hpp>

/**
 * Runs an Orbital Escalation server without a graphical application.
 *
 * @param argc The argument count, including the program name.
 * @param argv The program name and command-line arguments.
 * @return Zero for help or normal exit, or 1 if startup or the server reports
 * an exception.
 */
int32_t main(int32_t argc, char **argv) {
    try {
        std::uint32_t port = OrbitalServerPort;
        if (argc == 2 && std::string_view{argv[1]} == "--help") {
            std::cout
                << "Usage: svanes_game_orbital_escalation_server [--port N]\n"
                   "All clients join through this TCP port (default 45010).\n";
            return 0;
        }

        if (argc != 1) {
            if (argc != 3 || std::string_view{argv[1]} != "--port") {
                throw std::invalid_argument("Expected --port N or --help.");
            }

            const std::string_view text{argv[2]};
            const auto parsed =
                std::from_chars(text.data(), text.data() + text.size(), port);

            if (parsed.ec != std::errc{} ||
                parsed.ptr != text.data() + text.size() || port == 0 ||
                port > std::numeric_limits<std::uint16_t>::max()) {
                throw std::invalid_argument("Expected a port from 1 to 65535.");
            }
        }
        OrbitalEscalationServer server(
            svanes::AssetPath("assets/orbitalEscalation"),
            static_cast<std::uint16_t>(port));
        server.Run();
    } catch (const std::exception &error) {
        std::cerr << "Orbital Escalation server: " << error.what() << '\n';
        return 1;
    }
}

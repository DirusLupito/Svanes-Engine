#include "OrbitalEscalationServer.hpp"
#include <iostream>
#include <svanes/asset_path.hpp>

/**
 * Runs an Orbital Escalation server without a graphical application.
 *
 * @return Zero on normal exit, or 1 if the server reports an exception.
 */
int32_t main() {
    try {
        OrbitalEscalationServer server(
            svanes::AssetPath("assets/orbitalEscalation"));
        server.Run();
    } catch (const std::exception &error) {
        std::cerr << "Orbital Escalation server: " << error.what() << '\n';
        return 1;
    }
}

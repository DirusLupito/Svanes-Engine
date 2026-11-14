#pragma once

#include <svanes/network/clientserver/IServer.hpp>

namespace ghqcxx {

/**
 * Server implementation for the General Headquarters C++ game.
 */
class GHQCXXServer final : public svanes::IServer {
public:
    /**
     * Prints the server's startup message.
     *
     * @param context The engine services and configuration provided for
     * server initialization.
     */
    void Initialize(svanes::ServerContext &context) override;

    /**
     * Accepts the client and reports its assigned ID.
     *
     * @param context The client's request and reserved ID.
     *
     * @return An acceptance with no additional game data.
     */
    svanes::JoinResponse
    HandleJoinRequest(const svanes::ServerJoinContext &context) override;

    /**
     * Reports a client that has disconnected or timed out.
     *
     * @param world The server's registry.
     * @param client The ID of the disconnected client.
     */
    void HandleDisconnect(svanes::Registry &world,
                          svanes::ClientId client) override;

    /**
     * Leaves the server's state unchanged until there is gameplay to run.
     *
     * @param context The server's registry, networking, and gravity for this
     * update.
     */
    void Update(const svanes::ServerUpdateContext &context) override;
};

} // namespace ghqcxx

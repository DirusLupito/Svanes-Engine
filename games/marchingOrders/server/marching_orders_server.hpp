#pragma once

#include <svanes/network/clientserver/IServer.hpp>

namespace marching_orders {

/**
 * Server implementation for the marchingOrders game.
 */
class MarchingOrdersServer final : public svanes::IServer {
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
     * Runs one marchingOrders server update.
     *
     * @param context The engine services for this update.
     */
    void Update(const svanes::ServerUpdateContext &context) override;
};

} // namespace marching_orders

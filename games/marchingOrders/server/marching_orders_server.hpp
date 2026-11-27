#pragma once

#include "../scenario.hpp"

#include <svanes/network/clientserver/IServer.hpp>

#include <optional>

namespace marching_orders {

/**
 * Server implementation for the marchingOrders game.
 */
class MarchingOrdersServer final : public svanes::IServer {
public:
    /**
     * Prepares the server for a game session.
     *
     * @param context The engine services and configuration provided for
     * server initialization.
     */
    void Initialize(svanes::ServerContext &context) override;

    /**
     * Decides whether to admit a client to the game.
     *
     * @param context The client's request and reserved ID.
     *
     * @return The admission decision and the game data to send with it.
     */
    svanes::JoinResponse
    HandleJoinRequest(const svanes::ServerJoinContext &context) override;

    /**
     * Handles the end of a client's session.
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

    /**
     * Completes the scenario's physics step and publishes its state.
     *
     * @param context The engine services for the completed step.
     */
    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override;

private:
    // Authoritative match, created when the engine supplies its registry.
    std::optional<Scenario> scenario;
};

} // namespace marching_orders

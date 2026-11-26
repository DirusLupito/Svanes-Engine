#pragma once

#include "../unit.hpp"
#include "../utils/influence_map.hpp"

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
    void PhysicsUpdate(const svanes::ServerPhysicsContext &context) override;

private:
    // Test unit that receives movement orders from clients.
    std::optional<Unit> unit;

    // Number of columns in the control map.
    static constexpr std::uint32_t kControlMapWidth = 88;

    // Number of rows in the control map.
    static constexpr std::uint32_t kControlMapHeight = 88;

    // Side length of a control cell in world units.
    static constexpr std::uint32_t kControlCellSize = 10;

    // World coordinates where the first control cell begins.
    static constexpr svanes::Vector2D kControlMapOrigin{-440.0F, -440.0F};

    // Accumulated team influence in each cell, used to determine control tiers.
    InfluenceMap team_control_map{kControlMapWidth, kControlMapHeight,
                                  kControlCellSize, kControlMapOrigin};

    // Set once the first control stamp has been applied.
    bool control_stamped = false;

    // Simulation tics elapsed since the last control stamp.
    svanes::TicCount control_stamp_tics = 0;
};

} // namespace marching_orders

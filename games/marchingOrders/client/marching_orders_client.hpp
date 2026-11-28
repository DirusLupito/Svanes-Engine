#pragma once

#include "../scenario.hpp"

#include <svanes/entity.hpp>
#include <svanes/network/clientserver/IClient.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <cstdint>
#include <map>
#include <optional>

namespace marching_orders {

/**
 * Client implementation for the marchingOrders game.
 */
class MarchingOrdersClient final : public svanes::IClient {
public:
    /**
     * Initializes the client's local game state before the first frame.
     *
     * @param context The engine services and window dimensions provided for
     * client initialization.
     */
    void Initialize(svanes::ClientContext &context) override;

    /**
     * Creates the client's join request.
     *
     * @return The game-specific data to include in the engine's join request.
     */
    svanes::NetworkMessage CreateJoinRequest() override;

    /**
     * Handles the outcome of the attempt to join the game.
     *
     * @param result The server's response or the engine's timeout result.
     *
     * @throws std::runtime_error if the server rejects the request or the
     * join request times out.
     */
    void HandleJoinResult(const svanes::JoinResult &result) override;

    /**
     * Throws when an established connection to the server times out.
     *
     * @throws std::runtime_error to report the lost server connection.
     */
    void HandleDisconnect() override;

    /**
     * Processes player input and updates client-side game state before
     * rendering each frame.
     *
     * @param context The current frame's input and engine services.
     */
    void Update(const svanes::ClientFrameContext &context) override;

    /**
     * Reports whether the client has requested shutdown.
     *
     * @return True to end the frame loop, false to continue.
     */
    bool ShouldQuit() const override;

    /**
     * Provides the interval the engine uses to pace the client.
     *
     * @return The target interval between client frames, in microseconds.
     */
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    /**
     * Applies camera movement and cursor-centered zoom to the player's view.
     *
     * @param context The input, camera, output size, and elapsed frame time.
     */
    void UpdateCamera(const svanes::ClientFrameContext &context);

    /**
     * Updates unit icons and the control overlay from the current scenario.
     *
     * @param world The registry containing the client's render entities.
     */
    void UpdateScenarioView(svanes::Registry &world);

    // Match state received from the server.
    Scenario scenario{Terrain::Get()};

    // Local icon entities keyed by the unit IDs used in orders and snapshots.
    std::map<svanes::Entity, svanes::Entity> unit_icons;

    std::optional<svanes::Entity> selected_unit;

    // Texture used to draw units on the map.
    svanes::TextureHandle unit_texture{};

    // Entity that draws the team's control overlay on the terrain.
    svanes::Entity control_map_entity{};

    // Magnification relative to the view that fits the whole map.
    double zoom_factor = 1.0;

    // Seconds spent holding zoom in the current direction.
    double zoom_hold_seconds = 0.0;

    // Direction of zoom. Positive for zooming in, negative for zooming out, and
    // zero when idle.
    std::int8_t zoom_direction = 0;

    // Set when the client is ready to exit the frame loop.
    bool quit = false;
};

} // namespace marching_orders

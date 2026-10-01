#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/network/udp_msg_pipe.hpp>

#include "goose_session.hpp"
#include "goose_simulation.hpp"
#include "pause_menu.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

/**
 * How this process takes part in a world.
 * FIELDS:
 * - port: The local UDP port for peer-to-peer play.
 * - join_address: A player to join peer-to-peer, or empty to start a new world.
 * - server_host: A goose server to play on instead of peer-to-peer, or empty.
 */
struct GooseLaunch {
    std::uint16_t port;
    std::optional<svanes::UdpAddress> join_address;
    std::optional<std::string> server_host;
};

class GooseGame final : public svanes::IGame {
public:
    /**
     * @param launch Whether to start or join a peer-to-peer world, or play on a goose server.
     */
    explicit GooseGame(GooseLaunch launch);

    /**
     * Builds the starting world: gravity, the background, the static geometry, and
     * the entities the game begins with.
     *
     * @param context The game context supplying the registry, texture loader, and
     * the world gravity vector.
     */
    void Initialize(svanes::GameContext& context) override;

    /**
     * Advances the game by one frame.
     *
     * @param frame The frame context supplying the registry, input, camera, frame
     * delta, and output size.
     */
    void Update(const svanes::FrameContext& frame) override;

    /**
     * @return Whether the application should stop running.
     */
    bool ShouldQuit() const override;

    /**
     * On a goose server, paces the loop at the menu's loop rate, so a slower
     * client also sends input less often. Peer-to-peer play runs unpaced, since
     * a slower peer would hold back everyone sharing its lockstep.
     * @return The interval between frames, or zero for no pacing.
     */
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    /**
     * Keeps the camera on the local player and sizes the sky to the view.
     * @param frame The current rendering context.
     * @param player The local goose entity to follow.
     */
    void UpdateCamera(const svanes::FrameContext& frame, svanes::Entity player);

    /**
     * Prints a status line when it differs from the last one printed.
     * @param status The current status.
     */
    void ReportStatus(const std::string& status);

    GooseLaunch launch;
    std::unique_ptr<GooseSimulation> simulation;
    std::unique_ptr<GooseSession> session;
    std::unique_ptr<PauseMenu> menu;
    std::string last_network_status;

    svanes::Entity orb{};

    // the sky, kept centered on the camera and sized to cover the view every frame
    svanes::Entity background{};

    bool should_quit = false;
};

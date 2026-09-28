#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>

#include "enemy.hpp"
#include "goose.hpp"
#include "goose_network.hpp"
#include "goose_simulation.hpp"
#include "goose_rollback.hpp"

class ErikGame final : public svanes::IGame {
public:
    /**
     * Starts a new world open for others to join, or joins an existing one.
     * @param port The local UDP port this process listens on.
     * @param join_address A player already in the world to join, or empty to start a new world.
     */
    explicit ErikGame(std::uint16_t port, std::optional<svanes::UdpAddress> join_address = std::nullopt);

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

private:
    /**
     * Keeps the camera on the local player and sizes the sky to the view.
     * @param frame The current rendering context.
     * @param player The local goose entity to follow.
     */
    void UpdateCamera(const svanes::FrameContext& frame, svanes::Entity player);

    /**
     * Advances the join handshake and, once admitted, waits for the world snapshot.
     * @param frame The current frame, whose world receives the joined players.
     */
    void UpdateJoining(const svanes::FrameContext& frame);

    /**
     * Prints a status line when it differs from the last one printed.
     * @param status The current status.
     */
    void ReportStatus(const std::string& status);

    std::uint16_t port;
    std::optional<svanes::UdpAddress> join_address;
    std::unique_ptr<GooseJoin> join;
    std::unique_ptr<GooseNetwork> network;
    std::unique_ptr<GooseSimulation> simulation;
    std::unique_ptr<GooseRollback> rollback;
    std::string last_network_status;
    svanes::TicCount network_diagnostic_tics = 0;

    svanes::Entity orb{};

    // the sky, kept centered on the camera and sized to cover the view every frame
    svanes::Entity background{};

    bool should_quit = false;
};

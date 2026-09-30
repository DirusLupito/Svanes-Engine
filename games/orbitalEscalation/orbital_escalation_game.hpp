#pragma once

#include "orbital_simulation.hpp"

#include <svanes/entity.hpp>
#include <svanes/game.hpp>

#include <optional>
#include <string>

// Separate defaults allow a host and joiner to run on one computer.
inline constexpr std::uint16_t OrbitalHostPort = 45000;
inline constexpr std::uint16_t OrbitalJoinPort = 45001;

// Use confirmed inputs and limit catch-up work so a slow frame slows gameplay
// instead of running several overdue ticks.
inline constexpr svanes::SyncSettings OrbitalSyncSettings{
    .step_tics = OrbitalStepTics,
    .prediction_ticks = 0,
    .steps_per_frame = 1,
    .max_backlog_steps = 1,
};

/**
 * Top level container for the Orbital Escalation game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class OrbitalEscalationGame final : public svanes::IGame {
public:
    /**
     * Configures the game to start a world or join an existing one.
     *
     * @param port The local UDP port to listen on.
     * @param join_address An existing peer to join, or std::nullopt to start
     * a new world.
     */
    explicit OrbitalEscalationGame(
        std::uint16_t port = OrbitalHostPort,
        std::optional<svanes::UdpAddress> join_address = std::nullopt);

    /**
     * Initializes the game with the provided context.
     * @param context The context for the game, providing access to the
     * TextureManager.
     */
    void Initialize(svanes::GameContext &context) override;

    /**
     * Game specific update logic. Called by the engine once per frame.
     * @param frame The context for the current frame, providing access to the
     * InputManager and the time elapsed since the last frame.
     */
    void Update(const svanes::FrameContext &frame) override;

    /**
     * Checks whether the network session is ready for the game to close.
     *
     * @return true if the game should close, false otherwise.
     */
    bool ShouldQuit() const override;

private:
    // The local UDP port used to receive peer traffic.
    std::uint16_t port;

    // The peer to contact when joining, or no address when starting a world.
    std::optional<svanes::UdpAddress> join_address;

    // Owns the peer connections and agreed roster.
    std::unique_ptr<svanes::PeerGroup> network;

    // Owns and advances the gameplay world, independently of rendering.
    std::unique_ptr<OrbitalSimulation> simulation;

    // Coordinates player inputs and simulation steps across peers.
    std::unique_ptr<svanes::InputSync<OrbitalInput, std::uint8_t>> sync;
    
    // Last reported status and roster revision, to avoid repeating messages.
    std::string last_network_status;
    
    std::uint64_t last_roster_revision = 0;

    // The timeline used to animate the pause label.
    svanes::Timeline pause_timeline;

    // The text label displayed while gameplay is paused.
    svanes::Entity pause_label_entity = 0;

    // The background rectangle that follows the camera's view.
    svanes::Entity background_entity = 0;

    // Whether the session has allowed the game to close.
    bool should_quit = false;
};

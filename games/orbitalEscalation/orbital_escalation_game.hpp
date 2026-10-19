#pragma once

#include "orbital_simulation.hpp"

#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/network/clientserver/network_client.hpp>

#include <string>

/**
 * Top level container for the Orbital Escalation game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class OrbitalEscalationGame final : public svanes::IGame {
public:
    /**
     * Configures the game to play in a world controlled by a server.
     *
     * @param server_host The host running the server.
     * @param server_port The server's TCP joining port.
     * @throws std::invalid_argument if the host is empty or the port is zero.
     */
    OrbitalEscalationGame(std::string server_host, std::uint16_t server_port);
    /**
     * Initializes the game with the provided context.
     * @param context The context for the game, providing access to the
     * TextureManager.
     * @throws std::runtime_error if a server join request cannot be sent.
     * @throws zmq::error_t if requesting a server connection fails.
     */
    void Initialize(svanes::GameContext &context) override;

    /**
     * Game specific update logic. Called by the engine once per frame.
     * @param frame The context for the current frame, providing access to the
     * InputManager and the time elapsed since the last frame.
     */
    void Update(const svanes::FrameContext &frame) override;

    /**
     * Checks whether the game is ready to close.
     *
     * @return true if the game should close, false otherwise.
     */
    bool ShouldQuit() const override;

    /**
     * Determines the interval between frames in tics.
     * This method is called once per frame, allowing the game to control the
     * pace of the main loop. The engine will sleep for the remaining time
     * until the next frame, if any.
     *
     * @return The interval between frames in tics. A tic is one microsecond.
     * Return 0 to disable pacing and run as fast as possible.
     */
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    /**
     * Updates the displayed world from server snapshots and sends local
     * controls for the server to apply.
     *
     * @param frame The local input, camera, world, and elapsed frame time.
     * @throws std::invalid_argument if a snapshot or outgoing controls are
     * invalid.
     * @throws std::out_of_range if a snapshot references a missing object.
     * @throws zmq::error_t if joining or exchanging server messages fails.
     */
    void UpdateServerClient(const svanes::FrameContext &frame);

    // The authoritative server's host.
    std::string server_host;
    // The connection used to exchange controls and server snapshots.
    std::unique_ptr<svanes::NetworkClient> client;
    // The player controlled by this process, also used for camera tracking.
    std::uint32_t local_player = 0;
    svanes::Timeline client_loop_timeline;

    // The destination TCP port.
    std::uint16_t port;

    // Owns and advances the gameplay world, independently of rendering.
    std::unique_ptr<OrbitalSimulation> simulation;

    // The timeline used to animate the pause label.
    svanes::Timeline pause_timeline;

    // The text label displayed while gameplay is paused.
    svanes::Entity pause_label_entity = 0;

    // The background rectangle that follows the camera's view.
    svanes::Entity background_entity = 0;

    // Whether the game is ready to close.
    bool should_quit = false;
};

#pragma once

#include <svanes/entity.hpp>
#include <svanes/network/clientserver/IClient.hpp>

#include <string>

/**
 * Implements Orbital Escalation's client view and local controls.
 */
class OrbitalEscalationClient final : public svanes::IClient {
public:
    /**
     * Configures the game to play in a world controlled by a server.
     *
     * @param server_host The host running the server.
     * @param server_port The server's joining port.
     * @throws std::invalid_argument if the host is empty or the port is zero.
     */
    OrbitalEscalationClient(std::string server_host, std::uint16_t server_port);

    /**
     * Prepares the client's camera, background, and user interface.
     * @param context The engine services used to initialize the client.
     */
    void Initialize(svanes::ClientContext &context) override;

    /**
     * Creates the game's join request for the server.
     * @return The game specific join request bytes.
     */
    svanes::NetworkMessage CreateJoinRequest() override;

    /**
     * Reports the outcome of joining the server.
     * @param result The admission decision or timeout reported by the engine.
     */
    void HandleJoinResult(const svanes::JoinResult &result) override;

    /**
     * Updates the client's view and local controls once per frame.
     * @param frame The context for the current frame, providing access to the
     * InputManager and the time elapsed since the last frame.
     */
    void Update(const svanes::ClientFrameContext &frame) override;

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
    // The authoritative server's host.
    std::string server_host;

    // The timeline controlling the client's frame rate.
    svanes::Timeline client_loop_timeline;

    // The server's joining port.
    std::uint16_t port;

    // The timeline used to animate the pause label.
    svanes::Timeline pause_timeline;

    // The text label displayed while gameplay is paused.
    svanes::Entity pause_label_entity = 0;

    // The background rectangle that follows the camera's view.
    svanes::Entity background_entity = 0;

    // Whether the game is ready to close.
    bool should_quit = false;
};

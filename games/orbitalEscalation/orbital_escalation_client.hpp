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
     * @param receive_address The local address for receiving server messages.
     * @param reply_address The address the server can reach to reply to this
     * client.
     * @param server_address The server's address for join requests.
     * @throws std::invalid_argument if an address is empty.
     */
    OrbitalEscalationClient(std::string receive_address,
                            std::string reply_address,
                            std::string server_address);

    /**
     * Starts joining the server and prepares the client's view.
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
    // The local address on which the client listens for server messages.
    std::string receive_address;

    // The address the server uses to reach this client.
    std::string reply_address;

    // The server's address.
    std::string server_address;

    // The timeline controlling the client's frame rate.
    svanes::Timeline client_loop_timeline;

    // The timeline used to animate the pause label.
    svanes::Timeline pause_timeline;

    // The text label displayed while gameplay is paused.
    svanes::Entity pause_label_entity = 0;

    // The background rectangle that follows the camera's view.
    svanes::Entity background_entity = 0;

    // Whether the game is ready to close.
    bool should_quit = false;
};

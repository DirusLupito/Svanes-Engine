#pragma once

#include <svanes/network/clientserver/IClient.hpp>

namespace marching_orders {

/**
 * Client implementation for the marchingOrders game.
 */
class MarchingOrdersClient final : public svanes::IClient {
public:
    /**
     * Prints the client's startup message.
     *
     * @param context The engine services and window dimensions provided for
     * client initialization.
     */
    void Initialize(svanes::ClientContext &context) override;

    /**
     * Creates the client's join request.
     *
     * @return An empty message to send with the engine's join request.
     */
    svanes::NetworkMessage CreateJoinRequest() override;

    /**
     * Reports the assigned client ID, or throws if the connection failed.
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
     * Checks whether the Escape key was pressed to quit the client.
     *
     * @param context The current frame's input and engine services.
     */
    void Update(const svanes::ClientFrameContext &context) override;

    /**
     * Returns whether the user has requested that the client quit.
     *
     * @return True if the user has pressed Escape, false otherwise.
     */
    bool ShouldQuit() const override;

    /**
     * Limits the client to approximately sixty frames per second.
     *
     * @return The interval between frames, in microseconds.
     */
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    // Set when the user presses Escape.
    bool quit = false;
};

} // namespace marching_orders

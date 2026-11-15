#pragma once

#include "Board.hpp"

#include <svanes/network/clientserver/IClient.hpp>

namespace ghqcxx {

/**
 * Client implementation for the General Headquarters C++ game.
 */
class GHQCXXClient final : public svanes::IClient {
public:
    /**
     * Creates an empty checkerboard with red piece previews above it and blue
     * piece previews below it, then centers the camera on the full display.
     *
     * @param context The engine services and window dimensions provided for
     * client initialization.
     */
    void Initialize(svanes::ClientContext &context) override;

    /**
     * Creates an empty join request. There is no player data to send yet.
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
     * Fits the board and pieces to the current window size, updates the
     * board's mouse controls, and checks whether Escape was pressed to quit.
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
    // The checkerboard and its mouse controls.
    Board board;

    // Set when the user presses Escape.
    bool quit = false;
};

} // namespace ghqcxx

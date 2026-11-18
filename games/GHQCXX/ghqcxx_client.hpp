#pragma once

#include "Board.hpp"
#include "BoardView.hpp"

#include <svanes/network/clientserver/IClient.hpp>
#include <svanes/network/network_message.hpp>

namespace ghqcxx {

/**
 * Client implementation for the General Headquarters C++ game.
 */
class GHQCXXClient final : public svanes::IClient {
public:
    /**
     * Initializes the client's game state and presentation.
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
     * Handles server messages and player input, and updates the client's
     * presentation.
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
    /**
     * Interprets a board click using the client's current selection.
     *
     * @param square The square the player clicked.
     * @param events The manager used to raise the chosen action.
     */
    void ClickSquare(Board::BoardCoordinate square,
                     svanes::EventManager &events);

    // The client's board state.
    Board board;

    // The board's visual presentation and mouse controls.
    BoardView boardView;

    // The piece selected by the client, on the board or in reserve.
    BoardSelection selection;

    // Whether the player is choosing a facing for the selected artillery.
    bool choosingFacing = false;

    // Set when the user presses Escape.
    bool quit = false;
};

} // namespace ghqcxx

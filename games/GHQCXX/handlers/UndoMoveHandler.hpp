#pragma once

#include "../Board.hpp"
#include "../events/UndoMoveEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

namespace svanes {
class NetworkClient;
} // namespace svanes

namespace ghqcxx {

class MoveHistory;

/**
 * Handles a player's decision to take back a move.
 */
class UndoMoveHandler final
    : public svanes::EventHandlerWrapper<UndoMoveEvent> {
public:
    /**
     * Creates a handler that restores the server's last position.
     *
     * @param board The server's board.
     * @param moveHistory The positions saved during the current turn.
     */
    UndoMoveHandler(Board &board, MoveHistory &moveHistory);

    /**
     * Creates a handler that sends the client's request to the server.
     *
     * @param network The client used to send the request.
     */
    explicit UndoMoveHandler(svanes::NetworkClient &network);
};

} // namespace ghqcxx

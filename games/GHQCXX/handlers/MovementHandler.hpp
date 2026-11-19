#pragma once

#include "../events/MovementEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

#include <vector>

namespace svanes {
class NetworkClient;
} // namespace svanes

namespace ghqcxx {

/**
 * Applies piece movement to a board.
 */
class MovementHandler final
    : public svanes::EventHandlerWrapper<MovementEvent> {
public:
    /**
     * Creates a handler that updates the server's board.
     *
     * @param board The board to update. Must outlive the handler.
     * @param moveHistory The positions saved during the current turn.
     */
    MovementHandler(Board &board, std::vector<Board> &moveHistory);

    /**
     * Creates a handler that updates the client's board and sends its moves
     * to the server.
     *
     * @param board The board to update. Must outlive the handler.
     * @param network The client used to send requests. Must outlive the
     * handler.
     */
    MovementHandler(Board &board, svanes::NetworkClient &network);
};

} // namespace ghqcxx

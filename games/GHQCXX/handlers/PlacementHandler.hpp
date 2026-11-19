#pragma once

#include "../events/PlacementEvent.hpp"
#include <svanes/events/event_handler_wrapper.hpp>

#include <vector>

namespace svanes {
class NetworkClient;
}

namespace ghqcxx {

/**
 * Applies reserve placements to a board.
 */
class PlacementHandler final
    : public svanes::EventHandlerWrapper<PlacementEvent> {
public:
    /**
     * Creates a handler that places reserve pieces on the server's board.
     *
     * @param board The server's board. Must outlive the handler.
     * @param moveHistory The positions saved during the current turn.
     */
    PlacementHandler(Board &board, std::vector<Board> &moveHistory);

    /**
     * Creates a handler that places reserve pieces on the client's board and
     * sends each placement to the server.
     *
     * @param board The client's board. Must outlive the handler.
     * @param network The client used to send the placement. Must outlive the
     * handler.
     */
    PlacementHandler(Board &board, svanes::NetworkClient &network);
};

} // namespace ghqcxx

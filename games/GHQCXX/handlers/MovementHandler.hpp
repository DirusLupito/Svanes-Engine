#pragma once

#include "../events/MovementEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

namespace svanes {
class NetworkClient;
} // namespace svanes

namespace ghqcxx {

/**
 * Handles requests to move or turn a piece on the board.
 */
class MovementHandler final
    : public svanes::EventHandlerWrapper<MovementEvent> {
public:
    /**
     * Creates a handler for piece movement and rotation requests.
     */
    MovementHandler();

    /**
     * Creates a client handler that forwards movement requests to the server.
     *
     * @param network The client used to send requests. Must outlive the
     * handler.
     */
    explicit MovementHandler(svanes::NetworkClient &network);
};

} // namespace ghqcxx

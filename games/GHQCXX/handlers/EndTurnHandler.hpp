#pragma once

#include "../events/EndTurnEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

namespace svanes {
class NetworkClient;
}

namespace ghqcxx {

class Board;

/**
 * Handles a player's decision to end the current turn.
 */
class EndTurnHandler final : public svanes::EventHandlerWrapper<EndTurnEvent> {
public:
    /**
     * Creates a handler that advances the server's turn.
     *
     * @param board The server's board. Must outlive the handler.
     */
    explicit EndTurnHandler(Board &board);

    /**
     * Creates a handler that sends the client's request to the server.
     *
     * @param network The client used to send the request. Must outlive the
     * handler.
     */
    explicit EndTurnHandler(svanes::NetworkClient &network);
};

} // namespace ghqcxx

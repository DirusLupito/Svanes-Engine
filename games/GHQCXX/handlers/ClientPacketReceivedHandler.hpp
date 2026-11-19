#pragma once

#include "../BoardView.hpp"
#include "../events/ClientPacketReceivedEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

namespace ghqcxx {

/**
 * Handles gameplay packets received by the client from the server.
 */
class ClientPacketReceivedHandler final
    : public svanes::EventHandlerWrapper<ClientPacketReceivedEvent> {
public:
    /**
     * Creates a handler that applies server updates to the client's board.
     *
     * @param board The board to update. Must outlive the handler.
     * @param selection The client's current selection. Must outlive the
     * handler.
     * @param choosingFacing Whether the client is choosing the selected
     * piece's facing. Must outlive the handler.
     * @param waitingForBoard Whether input is waiting for the server to
     * confirm an action. Must outlive the handler.
     */
    ClientPacketReceivedHandler(Board &board, BoardSelection &selection,
                                bool &choosingFacing, bool &waitingForBoard);
};

} // namespace ghqcxx

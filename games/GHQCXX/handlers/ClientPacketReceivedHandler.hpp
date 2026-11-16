#pragma once

#include "../Board.hpp"
#include "../events/ClientPacketReceivedEvent.hpp"

#include <svanes/events/event_handler_wrapper.hpp>

#include <optional>

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
     * @param selectedPiece The client's selected square. Must outlive the
     * handler.
     */
    ClientPacketReceivedHandler(
        Board &board, std::optional<Board::BoardCoordinate> &selectedPiece);
};

} // namespace ghqcxx

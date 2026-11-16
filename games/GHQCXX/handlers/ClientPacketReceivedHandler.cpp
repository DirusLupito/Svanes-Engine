#include "ClientPacketReceivedHandler.hpp"

#include "../GameMessage.hpp"
#include "../events/MovementEvent.hpp"

namespace ghqcxx {

ClientPacketReceivedHandler::ClientPacketReceivedHandler(
    Board &board, std::optional<Board::BoardCoordinate> &selectedPiece)
    : svanes::EventHandlerWrapper<ClientPacketReceivedEvent>(
          [&board, &selectedPiece](const ClientPacketReceivedEvent &event) {
              if (GetGameMessageType(event.message) ==
                  GameMessageType::BoardState) {
                  board = Board::FromMessage(event.message);
                  selectedPiece.reset();
                  return;
              }

              const auto movement = MovementEvent::FromMessage(event.message);
              // Apply the server's move directly; a local movement event
              // would send it back to the server.
              board.MovePiece(movement.from, movement.to);
              if (selectedPiece &&
                  selectedPiece->column == movement.from.column &&
                  selectedPiece->row == movement.from.row) {
                  selectedPiece = movement.to;
              }
          }) {}

} // namespace ghqcxx

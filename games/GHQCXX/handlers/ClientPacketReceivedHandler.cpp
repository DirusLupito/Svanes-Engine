#include "ClientPacketReceivedHandler.hpp"

#include "../GameMessage.hpp"
#include "../events/MovementEvent.hpp"

namespace ghqcxx {

ClientPacketReceivedHandler::ClientPacketReceivedHandler(
    Board &board, BoardSelection &selection)
    : svanes::EventHandlerWrapper<ClientPacketReceivedEvent>(
          [&board, &selection](const ClientPacketReceivedEvent &event) {
              if (GetGameMessageType(event.message) ==
                  GameMessageType::BoardState) {
                  board = Board::FromMessage(event.message);
                  selection = std::monostate{};
                  return;
              }

              const auto movement = MovementEvent::FromMessage(event.message);
              // Apply the server's move directly; a local movement event
              // would send it back to the server.
              board.MovePiece(movement.from, movement.to);
              if (const auto *square =
                      std::get_if<Board::BoardCoordinate>(&selection);
                  square && square->column == movement.from.column &&
                  square->row == movement.from.row) {
                  selection = movement.to;
              }
          }) {}

} // namespace ghqcxx

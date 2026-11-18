#include "ClientPacketReceivedHandler.hpp"

#include "../GameMessage.hpp"
#include "../events/EndTurnEvent.hpp"
#include "../events/MovementEvent.hpp"
#include "../events/PlacementEvent.hpp"

namespace ghqcxx {

ClientPacketReceivedHandler::ClientPacketReceivedHandler(
    Board &board, BoardSelection &selection)
    : svanes::EventHandlerWrapper<ClientPacketReceivedEvent>(
          [&board, &selection](const ClientPacketReceivedEvent &event) {
              switch (GetGameMessageType(event.message)) {
              case GameMessageType::EndTurn:
                  EndTurnEvent::FromMessage(event.message);
                  board.EndTurn();
                  return;
              case GameMessageType::BoardState:
                  board = Board::FromMessage(event.message);
                  selection = std::monostate{};
                  return;
              case GameMessageType::Placement: {
                  const auto placement =
                      PlacementEvent::FromMessage(event.message);
                  if (!board.CanPlaceFromReserve(placement.piece,
                                                 placement.to)) {
                      return;
                  }
                  board.PlaceFromReserve(placement.piece, placement.to);
                  if (const auto *reserve = std::get_if<Piece>(&selection);
                      reserve && board.GetReserveCount(*reserve) == 0) {
                      selection = std::monostate{};
                  }
                  return;
              }
              case GameMessageType::Movement: {
                  const auto movement =
                      MovementEvent::FromMessage(event.message);
                  // Apply the server's move directly; a local movement event
                  // would send it back to the server.
                  board.MovePiece(movement.from, movement.to, movement.facing);
                  if (const auto *square =
                          std::get_if<Board::BoardCoordinate>(&selection);
                      square && square->column == movement.from.column &&
                      square->row == movement.from.row) {
                      selection = movement.to;
                  }
                  return;
              }
              }
          }) {}

} // namespace ghqcxx

#include "UndoMoveHandler.hpp"

#include "../MoveHistory.hpp"

#include <svanes/network/clientserver/network_client.hpp>

namespace ghqcxx {

UndoMoveHandler::UndoMoveHandler(Board &board, MoveHistory &moveHistory)
    : svanes::EventHandlerWrapper<UndoMoveEvent>(
          [&board, &moveHistory](const UndoMoveEvent &) {
              moveHistory.Undo(board);
          }) {}

UndoMoveHandler::UndoMoveHandler(svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<UndoMoveEvent>(
          [&network](const UndoMoveEvent &event) {
              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

#include "UndoMoveHandler.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <stdexcept>

namespace ghqcxx {

UndoMoveHandler::UndoMoveHandler(Board &board, std::vector<Board> &moveHistory)
    : svanes::EventHandlerWrapper<UndoMoveEvent>(
          [&board, &moveHistory](const UndoMoveEvent &) {
              if (moveHistory.empty()) {
                  throw std::invalid_argument(
                      "There are no moves to undo this turn.");
              }
              board = moveHistory.back();
              moveHistory.pop_back();
          }) {}

UndoMoveHandler::UndoMoveHandler(svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<UndoMoveEvent>(
          [&network](const UndoMoveEvent &event) {
              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

#include "MovementHandler.hpp"

#include "../MoveHistory.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <iostream>

namespace ghqcxx {

MovementHandler::MovementHandler(Board &board, MoveHistory &moveHistory)
    : svanes::EventHandlerWrapper<MovementEvent>(
          [&board, &moveHistory](const MovementEvent &event) {
              Board next = board;
              next.MovePiece(event.from, event.to, event.facing,
                             event.captureSquare);

              // Rotating an artillery piece goes hand in hand with
              // moving it.
              const auto &piece = board.GetPiece(event.from);
              if (event.from != event.to ||
                  (piece->action != ActionState::ChoosingFacing &&
                   piece != next.GetPiece(event.from))) {
                  moveHistory.Save(board);
              }

              board = next;
              std::cout << "GHQCXX server moved a piece from ("
                        << static_cast<std::uint32_t>(event.from.column) << ", "
                        << static_cast<std::uint32_t>(event.from.row)
                        << ") to ("
                        << static_cast<std::uint32_t>(event.to.column) << ", "
                        << static_cast<std::uint32_t>(event.to.row) << ").\n"
                        << std::flush;
          }) {}

MovementHandler::MovementHandler(Board &board, svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<MovementEvent>(
          [&board, &network](const MovementEvent &event) {
              board.MovePiece(event.from, event.to, event.facing,
                              event.captureSquare);

              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

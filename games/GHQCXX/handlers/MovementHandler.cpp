#include "MovementHandler.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <iostream>

namespace ghqcxx {

MovementHandler::MovementHandler(Board &board)
    : svanes::EventHandlerWrapper<MovementEvent>(
          [&board](const MovementEvent &event) {
              board.MovePiece(event.from, event.to);
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
              board.MovePiece(event.from, event.to);

              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

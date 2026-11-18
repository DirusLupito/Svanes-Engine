#include "EndTurnHandler.hpp"

#include "../Board.hpp"

#include <svanes/network/clientserver/network_client.hpp>

namespace ghqcxx {

EndTurnHandler::EndTurnHandler(Board &board)
    : svanes::EventHandlerWrapper<EndTurnEvent>(
          [&board](const EndTurnEvent &) { board.EndTurn(); }) {}

EndTurnHandler::EndTurnHandler(svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<EndTurnEvent>(
          [&network](const EndTurnEvent &event) {
              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

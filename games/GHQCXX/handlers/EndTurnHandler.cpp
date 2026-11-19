#include "EndTurnHandler.hpp"

#include "../Board.hpp"
#include "../MoveHistory.hpp"

#include <svanes/network/clientserver/network_client.hpp>

namespace ghqcxx {

EndTurnHandler::EndTurnHandler(Board &board, MoveHistory &moveHistory)
    : svanes::EventHandlerWrapper<EndTurnEvent>(
          [&board, &moveHistory](const EndTurnEvent &) {
              board.EndTurn();
              moveHistory.Clear();
          }) {}

EndTurnHandler::EndTurnHandler(svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<EndTurnEvent>(
          [&network](const EndTurnEvent &event) {
              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

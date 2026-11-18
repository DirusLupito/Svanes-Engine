#include "PlacementHandler.hpp"
#include <svanes/network/clientserver/network_client.hpp>

namespace ghqcxx {

PlacementHandler::PlacementHandler(Board &board)
    : svanes::EventHandlerWrapper<PlacementEvent>(
          [&board](const PlacementEvent &event) {
              board.PlaceFromReserve(event.piece, event.to);
          }) {}

PlacementHandler::PlacementHandler(Board &board, svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<PlacementEvent>(
          [&board, &network](const PlacementEvent &event) {
              board.PlaceFromReserve(event.piece, event.to);
              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

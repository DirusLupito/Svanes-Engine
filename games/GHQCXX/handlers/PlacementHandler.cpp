#include "PlacementHandler.hpp"
#include <svanes/network/clientserver/network_client.hpp>

namespace ghqcxx {

PlacementHandler::PlacementHandler(Board &board,
                                   std::vector<Board> &moveHistory)
    : svanes::EventHandlerWrapper<PlacementEvent>(
          [&board, &moveHistory](const PlacementEvent &event) {
              Board next = board;
              next.PlaceFromReserve(event.piece, event.to);
              moveHistory.push_back(board);
              board = next;
          }) {}

PlacementHandler::PlacementHandler(Board &board, svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<PlacementEvent>(
          [&board, &network](const PlacementEvent &event) {
              board.PlaceFromReserve(event.piece, event.to);
              network.Send(event.ToMessage());
          }) {}

} // namespace ghqcxx

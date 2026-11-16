#include "MovementHandler.hpp"

#include <svanes/network/clientserver/network_client.hpp>

#include <cstddef>
#include <iostream>

namespace ghqcxx {

MovementHandler::MovementHandler()
    : svanes::EventHandlerWrapper<MovementEvent>(
          [](const MovementEvent &event) {
              std::cout << "GHQCXX received " << event.GetEventType() << ".\n"
                        << std::flush;
          }) {}

MovementHandler::MovementHandler(svanes::NetworkClient &network)
    : svanes::EventHandlerWrapper<MovementEvent>(
          [&network](const MovementEvent &event) {
              std::cout << "GHQCXX received " << event.GetEventType() << ".\n"
                        << std::flush;

              // Four coordinate bytes (from column/row, then to column/row)
              // followed by the facing. 0xff means no direction change.
              network.Send(svanes::NetworkMessage(
                  {static_cast<std::byte>(event.from.column),
                   static_cast<std::byte>(event.from.row),
                   static_cast<std::byte>(event.to.column),
                   static_cast<std::byte>(event.to.row),
                   event.facing ? static_cast<std::byte>(*event.facing)
                                : std::byte{0xff}}));
          }) {}

} // namespace ghqcxx

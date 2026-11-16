#include "ServerPacketReceivedHandler.hpp"

#include "../events/MovementEvent.hpp"

#include <svanes/events/event_manager.hpp>

#include <iostream>

namespace ghqcxx {

ServerPacketReceivedHandler::ServerPacketReceivedHandler(
    svanes::EventManager &events, svanes::NetworkServer &network,
    const std::vector<svanes::ClientId> &clients)
    : svanes::EventHandlerWrapper<ServerPacketReceivedEvent>(
          [&events, &network,
           &clients](const ServerPacketReceivedEvent &event) {
              std::cout
                  << "GHQCXX server received a gameplay packet from client "
                  << event.client << " (" << event.message.bytes.size()
                  << " bytes).\n"
                  << std::flush;

              events.TriggerEvent(MovementEvent::FromMessage(event.message));

              // Relay only after the move succeeds. Its sender has already
              // applied it locally, so sending it back would move it twice.
              for (const auto client : clients) {
                  if (client != event.client) {
                      network.Send(client, event.message);
                  }
              }
          }) {}

} // namespace ghqcxx

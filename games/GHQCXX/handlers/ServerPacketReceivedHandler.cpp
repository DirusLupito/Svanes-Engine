#include "ServerPacketReceivedHandler.hpp"

#include "../events/MovementEvent.hpp"

#include <svanes/events/event_manager.hpp>

#include <iostream>

namespace ghqcxx {

ServerPacketReceivedHandler::ServerPacketReceivedHandler(
    svanes::EventManager &events)
    : svanes::EventHandlerWrapper<ServerPacketReceivedEvent>(
          [&events](const ServerPacketReceivedEvent &event) {
              std::cout
                  << "GHQCXX server received a gameplay packet from client "
                  << event.client << " (" << event.message.bytes.size()
                  << " bytes).\n"
                  << std::flush;

              events.TriggerEvent(MovementEvent::FromMessage(event.message));
          }) {}

} // namespace ghqcxx

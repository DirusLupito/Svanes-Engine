#include "ServerPacketReceivedHandler.hpp"

#include <iostream>

namespace ghqcxx {

ServerPacketReceivedHandler::ServerPacketReceivedHandler()
    : svanes::EventHandlerWrapper<ServerPacketReceivedEvent>(
          [](const ServerPacketReceivedEvent &event) {
              std::cout
                  << "GHQCXX server received a gameplay packet from client "
                  << event.client << " (" << event.message.bytes.size()
                  << " bytes).\n"
                  << std::flush;
          }) {}

} // namespace ghqcxx

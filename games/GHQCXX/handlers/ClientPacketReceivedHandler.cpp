#include "ClientPacketReceivedHandler.hpp"

#include <iostream>

namespace ghqcxx {

ClientPacketReceivedHandler::ClientPacketReceivedHandler()
    : svanes::EventHandlerWrapper<ClientPacketReceivedEvent>(
          [](const ClientPacketReceivedEvent &event) {
              std::cout << "GHQCXX client received a gameplay packet ("
                        << event.message.bytes.size() << " bytes).\n"
                        << std::flush;
          }) {}

} // namespace ghqcxx

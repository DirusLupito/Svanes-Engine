#include "ServerPacketReceivedEvent.hpp"

#include <utility>

namespace ghqcxx {

ServerPacketReceivedEvent::ServerPacketReceivedEvent(
    svanes::ClientId client, svanes::NetworkMessage message)
    : client(client), message(std::move(message)) {}

} // namespace ghqcxx

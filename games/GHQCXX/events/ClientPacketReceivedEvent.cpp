#include "ClientPacketReceivedEvent.hpp"

#include <utility>

namespace ghqcxx {

ClientPacketReceivedEvent::ClientPacketReceivedEvent(
    svanes::NetworkMessage message)
    : message(std::move(message)) {}

} // namespace ghqcxx

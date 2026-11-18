#include "EndTurnEvent.hpp"

#include <stdexcept>

namespace ghqcxx {

svanes::NetworkMessage EndTurnEvent::ToMessage() const {
    return svanes::NetworkMessage(
        {static_cast<std::byte>(GameMessageType::EndTurn)});
}

EndTurnEvent EndTurnEvent::FromMessage(const svanes::NetworkMessage &message) {
    if (message.bytes.size() != sizeof(GameMessageType) ||
        GetGameMessageType(message) != GameMessageType::EndTurn) {
        throw std::invalid_argument("Expected a GHQ end-turn packet.");
    }
    return {};
}

} // namespace ghqcxx

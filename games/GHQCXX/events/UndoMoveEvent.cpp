#include "UndoMoveEvent.hpp"

#include <stdexcept>

namespace ghqcxx {

svanes::NetworkMessage UndoMoveEvent::ToMessage() const {
    return svanes::NetworkMessage(
        {static_cast<std::byte>(GameMessageType::UndoMove)});
}

UndoMoveEvent
UndoMoveEvent::FromMessage(const svanes::NetworkMessage &message) {
    if (message.bytes.size() != sizeof(GameMessageType) ||
        GetGameMessageType(message) != GameMessageType::UndoMove) {
        throw std::invalid_argument("Expected a GHQ undo packet.");
    }
    return {};
}

} // namespace ghqcxx

#include "GameMessage.hpp"

#include <svanes/network/message_serialization.hpp>

#include <stdexcept>

namespace ghqcxx {

GameMessageType GetGameMessageType(const svanes::NetworkMessage &message) {
    svanes::MessageReader reader(message);
    const auto type = static_cast<GameMessageType>(reader.ReadUint8());
    switch (type) {
    case GameMessageType::Movement:
    case GameMessageType::BoardState:
        return type;
    default:
        throw std::invalid_argument("Unknown GHQ gameplay message type.");
    }
}

} // namespace ghqcxx

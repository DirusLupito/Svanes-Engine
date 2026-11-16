#include "MovementEvent.hpp"

#include <svanes/network/message_serialization.hpp>

#include <stdexcept>

namespace ghqcxx {

MovementEvent::MovementEvent(Board::BoardCoordinate from,
                             Board::BoardCoordinate to,
                             std::optional<Facing> facing)
    : from(from), to(to), facing(facing) {}

svanes::NetworkMessage MovementEvent::ToMessage() const {
    // After the message type, coordinates are written column first, source
    // before destination.
    return svanes::NetworkMessage(
        {static_cast<std::byte>(GameMessageType::Movement),
         static_cast<std::byte>(from.column), static_cast<std::byte>(from.row),
         static_cast<std::byte>(to.column), static_cast<std::byte>(to.row),
         static_cast<std::byte>(facing ? static_cast<std::uint8_t>(*facing)
                                       : UnchangedFacing)});
}

MovementEvent
MovementEvent::FromMessage(const svanes::NetworkMessage &message) {
    if (message.bytes.size() != MessageSize) {
        throw std::invalid_argument("Invalid GHQ movement packet size.");
    }

    if (GetGameMessageType(message) != GameMessageType::Movement) {
        throw std::invalid_argument("Expected a GHQ movement packet.");
    }

    svanes::MessageReader reader(message);
    reader.ReadUint8();
    const Board::BoardCoordinate from{reader.ReadUint8(), reader.ReadUint8()};
    const Board::BoardCoordinate to{reader.ReadUint8(), reader.ReadUint8()};
    const auto direction = reader.ReadUint8();
    std::optional<Facing> facing;
    if (direction != UnchangedFacing) {
        if (direction > static_cast<std::uint8_t>(Facing::NorthWest)) {
            throw std::invalid_argument("Invalid GHQ movement facing.");
        }

        facing = static_cast<Facing>(direction);
    }

    return MovementEvent(from, to, facing);
}

} // namespace ghqcxx

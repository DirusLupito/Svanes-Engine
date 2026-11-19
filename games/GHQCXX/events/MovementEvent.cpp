#include "MovementEvent.hpp"

#include <svanes/network/message_serialization.hpp>

#include <stdexcept>

namespace ghqcxx {

MovementEvent::MovementEvent(
    Board::BoardCoordinate from, Board::BoardCoordinate to,
    std::optional<Facing> facing,
    std::optional<Board::BoardCoordinate> captureSquare)
    : from(from), to(to), facing(facing), captureSquare(captureSquare) {}

svanes::NetworkMessage MovementEvent::ToMessage() const {
    // After the message type, coordinates are written column first, source
    // before destination.
    svanes::NetworkMessage message(
        {static_cast<std::byte>(GameMessageType::Movement),
         static_cast<std::byte>(from.column), static_cast<std::byte>(from.row),
         static_cast<std::byte>(to.column), static_cast<std::byte>(to.row),
         static_cast<std::byte>(facing ? static_cast<std::uint8_t>(*facing)
                                       : UnchangedFacing)});
    if (captureSquare) {
        message.bytes.push_back(static_cast<std::byte>(captureSquare->column));
        message.bytes.push_back(static_cast<std::byte>(captureSquare->row));
    }

    return message;
}

MovementEvent
MovementEvent::FromMessage(const svanes::NetworkMessage &message) {
    if (message.bytes.size() != MessageSize &&
        message.bytes.size() != CaptureMessageSize) {
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

    std::optional<Board::BoardCoordinate> captureSquare;
    if (message.bytes.size() == CaptureMessageSize) {
        captureSquare =
            Board::BoardCoordinate{reader.ReadUint8(), reader.ReadUint8()};
    }

    return MovementEvent(from, to, facing, captureSquare);
}

} // namespace ghqcxx

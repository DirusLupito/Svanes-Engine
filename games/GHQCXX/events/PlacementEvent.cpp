#include "PlacementEvent.hpp"

#include <svanes/network/message_serialization.hpp>

#include <stdexcept>

namespace ghqcxx {

PlacementEvent::PlacementEvent(Piece piece, Board::BoardCoordinate to)
    : piece(piece), to(to) {}

svanes::NetworkMessage PlacementEvent::ToMessage() const {
    return svanes::NetworkMessage(
        {static_cast<std::byte>(GameMessageType::Placement),
         static_cast<std::byte>(piece.type), static_cast<std::byte>(piece.team),
         static_cast<std::byte>(to.column), static_cast<std::byte>(to.row)});
}

PlacementEvent
PlacementEvent::FromMessage(const svanes::NetworkMessage &message) {
    if (message.bytes.size() != MessageSize) {
        throw std::invalid_argument("Invalid GHQ placement packet size.");
    }
    if (GetGameMessageType(message) != GameMessageType::Placement) {
        throw std::invalid_argument("Expected a GHQ placement packet.");
    }

    svanes::MessageReader reader(message);
    reader.ReadUint8();
    const Piece piece{static_cast<PieceType>(reader.ReadUint8()),
                      static_cast<Team>(reader.ReadUint8())};
    const Board::BoardCoordinate to{reader.ReadUint8(), reader.ReadUint8()};
    return PlacementEvent(piece, to);
}

} // namespace ghqcxx

#pragma once

#include "../Board.hpp"
#include "../GameMessage.hpp"

#include <svanes/events/event.hpp>

#include <cstddef>
#include <string_view>

namespace ghqcxx {

/**
 * Represents the deployment of a reserve piece to a board square.
 */
class PlacementEvent final : public svanes::EventOf<PlacementEvent> {
public:
    // The event name for reserve placement.
    static constexpr std::string_view EventName = "ghqcxx.placement";

    /**
     * Creates a request to place a reserve piece on the board.
     *
     * @param piece The reserve piece to deploy.
     * @param to The requested destination.
     */
    PlacementEvent(Piece piece, Board::BoardCoordinate to);

    /**
     * Encodes the reserve piece and its destination in a gameplay message.
     *
     * @return The placement encoded for transmission.
     */
    svanes::NetworkMessage ToMessage() const;

    /**
     * Decodes a reserve placement from a received gameplay message.
     *
     * @param message The received placement message.
     *
     * @return The decoded placement.
     *
     * @throws std::invalid_argument if the message type or length is invalid.
     */
    static PlacementEvent FromMessage(const svanes::NetworkMessage &message);

    // The type and team of the reserve piece.
    Piece piece;

    // The requested destination square.
    Board::BoardCoordinate to;

private:
    // The size of a complete placement message in bytes.
    static constexpr std::size_t MessageSize =
        sizeof(GameMessageType) + sizeof(piece.type) + sizeof(piece.team) +
        sizeof(to.column) + sizeof(to.row);
};

} // namespace ghqcxx

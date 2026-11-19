#pragma once

#include "../Board.hpp"
#include "../GameMessage.hpp"
#include "../pieces/Piece.hpp"

#include <svanes/events/event.hpp>
#include <svanes/network/network_message.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace ghqcxx {

/**
 * Represents a request to move or turn a piece on the board.
 */
class MovementEvent final : public svanes::EventOf<MovementEvent> {
public:
    // The unique name identifying piece movement and rotation requests.
    static constexpr std::string_view EventName = "ghqcxx.movement";

    /**
     * Constructs an event describing a requested movement or rotation.
     *
     * @param from The square containing the piece to move.
     * @param to The requested destination square.
     * @param facing The requested final direction, or no value to keep the
     * current facing.
     * @param captureSquare The enemy square chosen for an infantry capture.
     */
    MovementEvent(
        Board::BoardCoordinate from, Board::BoardCoordinate to,
        std::optional<Facing> facing = std::nullopt,
        std::optional<Board::BoardCoordinate> captureSquare = std::nullopt);

    /**
     * Encodes this movement request for transmission.
     *
     * @return A message containing the requested movement.
     */
    svanes::NetworkMessage ToMessage() const;

    /**
     * Decodes a movement request from a received message.
     *
     * @param message The movement message to read.
     *
     * @return The decoded request.
     *
     * @throws std::invalid_argument if the message type, size, or facing is
     * invalid.
     */
    static MovementEvent FromMessage(const svanes::NetworkMessage &message);

    // The square containing the piece to move.
    Board::BoardCoordinate from;

    // The requested destination square. Equal to from for a rotation in place.
    Board::BoardCoordinate to;

    // The final facing after the action. Empty leaves the direction unchanged.
    std::optional<Facing> facing;

    // The enemy square captured by this move, if any.
    std::optional<Board::BoardCoordinate> captureSquare;

private:
    // The size of a movement message without a capture, in bytes.
    static constexpr std::size_t MessageSize =
        sizeof(GameMessageType) + sizeof(from.column) + sizeof(from.row) +
        sizeof(to.column) + sizeof(to.row) + sizeof(std::uint8_t);

    // The size of a movement message with the captured square, in bytes.
    static constexpr std::size_t CaptureMessageSize =
        MessageSize + sizeof(to.column) + sizeof(to.row);

    // The reserved facing byte that requests no direction change.
    static constexpr std::uint8_t UnchangedFacing =
        std::numeric_limits<std::uint8_t>::max();
};

} // namespace ghqcxx

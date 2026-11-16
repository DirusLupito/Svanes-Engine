#pragma once

#include "../Board.hpp"
#include "../pieces/Piece.hpp"

#include <svanes/events/event.hpp>

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
     */
    MovementEvent(Board::BoardCoordinate from, Board::BoardCoordinate to,
                  std::optional<Facing> facing = std::nullopt);

    // The square containing the piece to move.
    Board::BoardCoordinate from;

    // The requested destination square. Equal to from for a rotation in place.
    Board::BoardCoordinate to;

    // The final facing after the action. Empty leaves the direction unchanged.
    std::optional<Facing> facing;
};

} // namespace ghqcxx

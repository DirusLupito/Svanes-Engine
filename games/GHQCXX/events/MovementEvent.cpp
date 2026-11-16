#include "MovementEvent.hpp"

namespace ghqcxx {

MovementEvent::MovementEvent(Board::BoardCoordinate from,
                             Board::BoardCoordinate to,
                             std::optional<Facing> facing)
    : from(from), to(to), facing(facing) {}

} // namespace ghqcxx

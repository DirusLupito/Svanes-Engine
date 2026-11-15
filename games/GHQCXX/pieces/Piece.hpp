#pragma once

#include <cstdint>

namespace ghqcxx {

/**
 * Identifies the player a piece belongs to.
 *
 * MEMBERS:
 * - Red: The player starting in the upper-left corner.
 * - Blue: The player starting in the lower-right corner.
 */
enum class Team : std::uint8_t { Red, Blue };

/**
 * Identifies the kind of piece on a square.
 *
 * MEMBERS:
 * - Headquarters: The HQ that the opposing player must capture.
 * - BasicInfantry: Regular infantry.
 * - ArmoredInfantry: Infantry that can move two squares.
 * - ParachuteInfantry: Infantry that can paradrop from its back row.
 * - BasicArtillery: Regular artillery.
 * - HeavyArtillery: Artillery with a range of three squares.
 * - ArmoredArtillery: Artillery that can move two squares.
 */
enum class PieceType : std::uint8_t {
    Headquarters,
    BasicInfantry,
    ArmoredInfantry,
    ParachuteInfantry,
    BasicArtillery,
    HeavyArtillery,
    ArmoredArtillery
};

/**
 * Describes a piece's movement before the board checks its surroundings.
 * Ordinary movement follows any of the eight directions, with all steps
 * taken in the same direction.
 *
 * FIELDS:
 * - maxSteps: The maximum number of squares traveled in one ordinary move.
 * - canParadrop: Whether the piece can instead jump to any empty square
 * when starting on its own back row.
 */
struct MovementRules {
    std::uint8_t maxSteps;
    bool canParadrop;
};

/**
 * Represents a piece belonging to one player and provides its movement rules.
 * Its square and surrounding pieces are managed by the board.
 *
 * FIELDS:
 * - type: The kind of piece.
 * - team: The player the piece belongs to.
 */
struct Piece {
    PieceType type;
    Team team;

    /**
     * Reports the movement allowed by this piece's type. Armored pieces can
     * travel up to two squares; all other pieces travel one. Parachute
     * infantry also has the option of a paradrop from its own back row.
     *
     * @return The piece's movement distance and paradrop capability.
     *
     * @throws std::invalid_argument if the piece type is invalid.
     */
    MovementRules GetMovementRules() const;
};

} // namespace ghqcxx

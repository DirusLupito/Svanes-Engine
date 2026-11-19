#pragma once

#include <cstddef>
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

// The number of consecutive team values, for storage indexed by Team.
inline constexpr std::size_t TeamCount =
    static_cast<std::size_t>(Team::Blue) + 1;

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

// The number of consecutive piece types, for storage indexed by PieceType.
inline constexpr std::size_t PieceTypeCount =
    static_cast<std::size_t>(PieceType::ArmoredArtillery) + 1;

/**
 * Identifies a piece's facing on the board. North points toward the top row.
 *
 * MEMBERS:
 * - North: Toward the top of the board.
 * - NorthEast: Diagonally toward the top-right.
 * - East: Toward the right of the board.
 * - SouthEast: Diagonally toward the bottom-right.
 * - South: Toward the bottom of the board.
 * - SouthWest: Diagonally toward the bottom-left.
 * - West: Toward the left of the board.
 * - NorthWest: Diagonally toward the top-left.
 */
enum class Facing : std::uint8_t {
    North,
    NorthEast,
    East,
    SouthEast,
    South,
    SouthWest,
    West,
    NorthWest
};

/**
 * Tracks a piece's action for the current turn.
 *
 * MEMBERS:
 * - Ready: The piece can take an action.
 * - ChoosingFacing: The piece has moved and can still set its facing
 * (artillery).
 * - Exhausted: The piece has finished its action.
 */
enum class ActionState : std::uint8_t { Ready, ChoosingFacing, Exhausted };

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
 * - facing: The direction the piece points on the board.
 * - action: The piece's action state this turn.
 */
struct Piece {
    PieceType type;
    Team team;
    Facing facing = team == Team::Red ? Facing::South : Facing::North;
    ActionState action = ActionState::Ready;

    /**
     * Compares the state of two pieces.
     *
     * @param other The piece to compare.
     * @return True if both pieces have the same state.
     */
    bool operator==(const Piece &other) const = default;

    /**
     * Checks whether the piece can engage opposing infantry.
     *
     * @return True for basic, armored, and parachute infantry.
     */
    bool IsInfantry() const;

    /**
     * Reports how far the piece can bombard along its facing.
     *
     * @return The number of squares in range, or zero for a non-artillery
     * piece.
     */
    std::uint8_t GetFireRange() const;

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

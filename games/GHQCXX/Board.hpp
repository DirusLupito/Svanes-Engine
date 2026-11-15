#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace ghqcxx {

/**
 * Represents the state of the GHQ board and owns the pieces occupying its
 * squares.
 */
class Board final {
public:
    // The board has eight rows and columns.
    static constexpr std::int32_t Size = 8;

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
     * Represents a piece belonging to one player. Its square is determined
     * by where the board stores it.
     *
     * FIELDS:
     * - type: The kind of piece.
     * - team: The player the piece belongs to.
     */
    struct Piece {
        PieceType type;
        Team team;
    };

    /**
     * Identifies a square on the board, counting from the top-left corner.
     *
     * FIELDS:
     * - column: The zero-based column, increasing to the right.
     * - row: The zero-based row, increasing downward.
     */
    struct BoardCoordinate {
        std::uint8_t column;
        std::uint8_t row;
    };

    /**
     * Sets up both players' starting pieces. Call once before placing
     * additional pieces.
     *
     * @throws std::logic_error if the board has already been initialized.
     */
    void Initialize();

    /**
     * Places a piece on an empty square and keeps it as part of the board.
     *
     * @param piece The type and team of the piece to place.
     * @param position The destination square.
     *
     * @throws std::logic_error if the board has not been initialized or the
     * square is already occupied.
     * @throws std::out_of_range if the position is outside the board.
     * @throws std::invalid_argument if the piece type or team is invalid.
     */
    void PlacePiece(Piece piece, BoardCoordinate position);

    /**
     * Looks up the piece occupying a square.
     *
     * @param position The square to inspect.
     *
     * @return The board's piece at that square, or no value if it is empty.
     *
     * @throws std::out_of_range if the position is outside the board.
     */
    const std::optional<Piece> &GetPiece(BoardCoordinate position) const;

private:
    // Indexed by row, then column. An empty square has no piece.
    std::array<std::array<std::optional<Piece>, Size>, Size> pieces;

    // The starting formation is installed once before additional placements.
    bool initialized = false;
};

} // namespace ghqcxx

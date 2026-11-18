#pragma once

#include "pieces/Piece.hpp"

#include <svanes/network/network_message.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ghqcxx {

/**
 * Represents the state of the GHQ board and owns the pieces occupying its
 * squares and the quantities remaining in reserve.
 */
class Board final {
public:
    // The board has eight rows and columns.
    static constexpr std::int32_t Size = 8;

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
     * Sets up both players' starting pieces and reserves. Call once before
     * placing additional pieces.
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
     * Moves a piece to an available destination, leaving its old square empty.
     *
     * @param from The square containing the piece.
     * @param to The destination square.
     *
     * @throws std::out_of_range if either square is outside the board.
     * @throws std::invalid_argument if the move is not allowed.
     */
    void MovePiece(BoardCoordinate from, BoardCoordinate to);

    /**
     * Encodes the board's current state for transmission.
     *
     * @return A message containing every square's contents and both reserves.
     */
    svanes::NetworkMessage ToMessage() const;

    /**
     * Restores a board from an authoritative state message.
     *
     * @param message The board state to read.
     *
     * @return The complete board, ready for use without initialization.
     *
     * @throws std::invalid_argument if the message is not a valid board state.
     */
    static Board FromMessage(const svanes::NetworkMessage &message);

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

    /**
     * Reports how many pieces of a type remain in a team's reserve.
     *
     * @param piece The type and team to inspect.
     *
     * @return The remaining quantity, or zero when none are in reserve.
     *
     * @throws std::out_of_range if the piece type or team is invalid.
     */
    std::uint8_t GetReserveCount(Piece piece) const;

    /**
     * Sets how many pieces of a type remain in a team's reserve.
     *
     * @param piece The type and team to update.
     * @param count The remaining quantity.
     *
     * @throws std::logic_error if the board has not been initialized.
     * @throws std::out_of_range if the piece type or team is invalid.
     */
    void SetReserveCount(Piece piece, std::uint8_t count);

    /**
     * Finds destinations allowed by the piece's movement rules, board edges,
     * and occupied squares.
     *
     * @param position The square containing the piece to inspect.
     *
     * @return The available destinations, or an empty list if the square is
     * empty or the piece has no available destination.
     *
     * @throws std::out_of_range if the position is outside the board.
     */
    std::vector<BoardCoordinate>
    GetMovementOptions(BoardCoordinate position) const;

private:
    // Indexed by row, then column. An empty square has no piece.
    std::array<std::array<std::optional<Piece>, Size>, Size> pieces;

    // The number of pieces each team can still deploy, by piece type.
    std::array<std::array<std::uint8_t, PieceTypeCount>, TeamCount> reserves{};

    // The starting formation is installed once before additional placements.
    bool initialized = false;
};

} // namespace ghqcxx

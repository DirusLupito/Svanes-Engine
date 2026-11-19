#pragma once

#include "pieces/Piece.hpp"

#include <svanes/network/network_message.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
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

        /**
         * Compares two board positions.
         *
         * @param other The position to compare.
         * @return True if both coordinates identify the same square.
         */
        bool operator==(const BoardCoordinate &other) const = default;
    };

    /**
     * Sets up both players' starting pieces and reserves. Call once before
     * placing additional pieces.
     *
     * @throws std::logic_error if the board has already been initialized.
     */
    void Initialize();

    /**
     * Reports which team is taking its turn.
     *
     * @return The active team.
     */
    Team GetActiveTeam() const {
        return turnNumber % 2 == 1 ? Team::Blue : Team::Red;
    }

    /**
     * Reports the current turn, counting from Blue's opening turn.
     *
     * @return The one-based turn number.
     */
    std::uint32_t GetTurnNumber() const { return turnNumber; }

    /**
     * Checks whether a team has any actions left this turn.
     *
     * @param team The team whose actions to count.
     * @return True if fewer than three of its pieces have acted.
     */
    bool HasActionsRemaining(Team team) const;

    /**
     * Counts the actions a team can still take this turn.
     *
     * @param team The team whose actions to count.
     * @return The number of actions remaining.
     */
    std::uint8_t GetActionsRemaining(Team team) const;

    /**
     * Passes the turn to the other team and resolves its artillery captures.
     */
    void EndTurn();

    /**
     * Places a piece on an empty square and keeps it as part of the board.
     *
     * @param piece The type and team of the piece to place.
     * @param position The destination square.
     *
     * @throws std::logic_error if the board has not been initialized or the
     * square is already occupied.
     * @throws std::out_of_range if the position is outside the board.
     * @throws std::invalid_argument if the piece has invalid state.
     */
    void PlacePiece(Piece piece, BoardCoordinate position);

    /**
     * Checks whether a reserve piece can enter the board on a square.
     *
     * @param piece The type and team to deploy.
     * @param position The proposed destination.
     * @return True if the piece is available and the square is empty on its
     * team's back row.
     * @throws std::out_of_range if the piece type or team is invalid.
     */
    bool CanPlaceFromReserve(Piece piece, BoardCoordinate position) const;

    /**
     * Transfers one piece from its team's reserve to the board.
     *
     * @param piece The type and team to deploy.
     * @param position The destination square.
     * @throws std::invalid_argument if the piece cannot be placed there.
     * @throws std::out_of_range if the piece type or team is invalid.
     */
    void PlaceFromReserve(Piece piece, BoardCoordinate position);

    /**
     * Moves a piece or turns it in place.
     *
     * @param from The square containing the piece.
     * @param to The destination square, or the source square to turn in place.
     * @param facing The piece's final direction. Omit to keep its facing.
     *
     * @throws std::out_of_range if either square is outside the board.
     * @throws std::invalid_argument if the move is not allowed.
     */
    void MovePiece(BoardCoordinate from, BoardCoordinate to,
                   std::optional<Facing> facing = std::nullopt);

    /**
     * Checks whether a piece can move to a destination.
     *
     * @param from The square containing the piece.
     * @param to The proposed destination.
     *
     * @return True if the destination is one of the piece's movement options.
     */
    bool CanMovePiece(BoardCoordinate from, BoardCoordinate to) const;

    /**
     * Finds the squares an artillery piece could bombard from a position.
     *
     * @param position The square containing the piece.
     *
     * @return Each covered board square and the facing that would cover it.
     */
    std::vector<std::pair<BoardCoordinate, Facing>>
    GetFireOptions(BoardCoordinate position) const;

    /**
     * Encodes the board's current state for transmission.
     *
     * @return The encoded board state.
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
     * Looks up the infantry engaged with the piece on a square.
     *
     * @param position The square to inspect.
     * @return The opposing infantry's square, or no value if unengaged.
     * @throws std::out_of_range if the position is outside the board.
     */
    std::optional<BoardCoordinate>
    GetEngagedOpponent(BoardCoordinate position) const;

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
     * Finds the squares a piece can reach from its current position.
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
    // A set of board squares indexed by row, then column.
    using SquareMask = std::array<std::array<bool, Size>, Size>;

    /**
     * Finds the squares under fire from a team's artillery.
     *
     * @param team The team firing.
     * @return The squares covered by its guns in their current facings.
     */
    SquareMask GetBombardment(Team team) const;

    /**
     * Pairs as many opposing infantry as possible and turns them to face
     * each other.
     */
    void UpdateEngagements();

    /**
     * Finds a partner for a Red infantry, moving existing engagements to
     * other neighbors if needed.
     *
     * @param red The Red infantry's square.
     * @param visited The Blue squares already considered in this search.
     * @return True if the infantry found a partner.
     */
    bool TryEngage(BoardCoordinate red, SquareMask &visited);

    /**
     * Ends the free facing choice from a previous artillery move.
     */
    void FinishArtilleryMove();

    // The current turn number. Blue takes the first turn.
    std::uint32_t turnNumber = 1;

    // Indexed by row, then column. An empty square has no piece.
    std::array<std::array<std::optional<Piece>, Size>, Size> pieces;

    // The number of pieces each team can still deploy, by piece type.
    std::array<std::array<std::uint8_t, PieceTypeCount>, TeamCount> reserves{};

    // The starting formation is installed once before additional placements.
    bool initialized = false;
};

} // namespace ghqcxx

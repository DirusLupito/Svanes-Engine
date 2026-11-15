#pragma once

#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace svanes {
class Camera2D;
class InputManager;
class Registry;
} // namespace svanes

namespace ghqcxx {

/**
 * Represents the GHQ board and owns the pieces occupying its squares.
 * Provides placement and lookup by grid coordinate, along with the board's
 * display and mouse interaction. The engine registry stores the visuals.
 */
class Board final {
public:
    // The board has eight rows and columns, with one world unit per square.
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
     * Sets up the board and both players' starting pieces. Call once before
     * placing additional pieces or updating the board.
     *
     * @param world The registry that owns the board's visual entities.
     *
     * @throws std::logic_error if the board has already been initialized.
     */
    void Initialize(svanes::Registry &world);

    /**
     * Places a piece on an empty square. The board keeps the piece and creates
     * its visuals centered on that square, facing toward the opposing side.
     *
     * @param world The registry used to initialize the board.
     * @param piece The type and team of the piece to place.
     * @param position The destination square.
     *
     * @throws std::logic_error if the board has not been initialized or the
     * square is already occupied.
     * @throws std::out_of_range if the position is outside the board.
     * @throws std::invalid_argument if the piece type or team is invalid.
     */
    void PlacePiece(svanes::Registry &world, Piece piece,
                    BoardCoordinate position);

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
     * Returns the color used to display a team's pieces, including previews.
     *
     * @param team The player whose color is needed.
     *
     * @return NC State red for Red, or Carolina blue for Blue.
     *
     * @throws std::invalid_argument if the team is invalid.
     */
    static svanes::Color GetTeamColor(Team team);

    /**
     * Highlights the square under the mouse and prints its (column, row) on a
     * left click. Coordinates start at (0, 0) in the top-left corner. Clicks
     * outside the board are ignored.
     *
     * @param world The registry used to initialize the board.
     * @param input The current frame's mouse position and button states.
     * @param camera The camera used to draw the board, already fitted to the
     * current window size.
     */
    void Update(svanes::Registry &world, const svanes::InputManager &input,
                const svanes::Camera2D &camera);

private:
    // Indexed by row, then column. An empty square has no piece.
    std::array<std::array<std::optional<Piece>, Size>, Size> pieces;

    // Placement requires the board's visual entities to exist first.
    bool initialized = false;

    // The square under the mouse, or no value when the mouse is off the board.
    std::optional<BoardCoordinate> hoveredSquare;

    // A translucent rectangle moved over the hovered square. Its alpha is
    // zero when no board square is hovered.
    svanes::Entity hoverHighlight = 0;
};

} // namespace ghqcxx

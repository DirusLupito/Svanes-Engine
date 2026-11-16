#pragma once

#include "Board.hpp"

#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <array>
#include <optional>
#include <vector>

namespace svanes {
class Camera2D;
class InputManager;
class Registry;
} // namespace svanes

namespace ghqcxx {

/**
 * Represents the client's interactive view of a GHQ board.
 */
class BoardView final {
public:
    /**
     * Creates the board's visuals from its current pieces. Call once before
     * updating the view.
     *
     * @param world The registry that owns the view's visual entities.
     * @param board The board whose current setup will be displayed.
     *
     * @throws std::logic_error if the view has already been initialized.
     */
    void Initialize(svanes::Registry &world, const Board &board);

    /**
     * Synchronizes the displayed pieces with the board's state.
     *
     * @param world The registry used to initialize the view.
     * @param board The board whose pieces will be displayed.
     */
    void UpdatePieces(svanes::Registry &world, const Board &board);

    /**
     * Updates mouse feedback and reports a clicked square. Coordinates start
     * at (0, 0) in the top-left corner. Clicks outside the board are ignored.
     *
     * @param world The registry used to initialize the view.
     * @param input The current frame's mouse position and button states.
     * @param camera The camera used to draw the board, already fitted to the
     * current window size.
     *
     * @return The square left-clicked this frame, or no value if no board
     * square was clicked.
     */
    std::optional<Board::BoardCoordinate>
    Update(svanes::Registry &world, const svanes::InputManager &input,
           const svanes::Camera2D &camera);

    /**
     * Highlights the selected square and its available movement destinations.
     *
     * @param world The registry used to initialize the view.
     * @param board The board used to find the selected piece's movement
     * options.
     * @param selectedSquare The occupied square selected by the client, or
     * no value to clear the selection feedback.
     */
    void
    ShowSelection(svanes::Registry &world, const Board &board,
                  const std::optional<Board::BoardCoordinate> &selectedSquare);

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

private:
    bool initialized = false;

    // The pieces currently displayed, indexed by row, then column.
    std::array<std::array<std::optional<Piece>, Board::Size>, Board::Size>
        displayedPieces;

    // The visual entities belonging to each square's displayed piece.
    std::array<std::array<std::vector<svanes::Entity>, Board::Size>,
               Board::Size>
        pieceEntities;

    // The square under the mouse, or no value when the mouse is off the board.
    std::optional<Board::BoardCoordinate> hoveredSquare;

    // A translucent rectangle moved over the hovered square. Its alpha is
    // zero when no board square is hovered.
    svanes::Entity hoverHighlight = 0;

    // A highlight rectangle behind the selected piece.
    svanes::Entity selectionHighlight = 0;

    // Green destination circles, indexed by row, then column.
    std::array<std::array<svanes::Entity, Board::Size>, Board::Size>
        movementMarkers{};
};

} // namespace ghqcxx

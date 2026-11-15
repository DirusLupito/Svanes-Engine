#pragma once

#include "Board.hpp"

#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <optional>

namespace svanes {
class Camera2D;
class InputManager;
class Registry;
} // namespace svanes

namespace ghqcxx {

/**
 * Represents the client's view of a board. Provides the checkerboard and
 * piece display, along with hover feedback and click coordinates.
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
     * Highlights the square under the mouse and prints its (column, row) on a
     * left click. Coordinates start at (0, 0) in the top-left corner. Clicks
     * outside the board are ignored.
     *
     * @param world The registry used to initialize the view.
     * @param input The current frame's mouse position and button states.
     * @param camera The camera used to draw the board, already fitted to the
     * current window size.
     */
    void Update(svanes::Registry &world, const svanes::InputManager &input,
                const svanes::Camera2D &camera);

    /**
     * Returns the color used to display a team's pieces, including previews.
     *
     * @param team The player whose color is needed.
     *
     * @return NC State red for Red, or Carolina blue for Blue.
     *
     * @throws std::invalid_argument if the team is invalid.
     */
    static svanes::Color GetTeamColor(Board::Team team);

private:
    bool initialized = false;

    // The square under the mouse, or no value when the mouse is off the board.
    std::optional<Board::BoardCoordinate> hoveredSquare;

    // A translucent rectangle moved over the hovered square. Its alpha is
    // zero when no board square is hovered.
    svanes::Entity hoverHighlight = 0;
};

} // namespace ghqcxx

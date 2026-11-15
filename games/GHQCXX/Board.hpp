#pragma once

#include <svanes/entity.hpp>

#include <cstdint>
#include <optional>

namespace svanes {
class Camera2D;
class InputManager;
class Registry;
} // namespace svanes

namespace ghqcxx {

/**
 * Represents the client's view of the GHQ board. Provides the view
 * display and interaction with the board.
 */
class Board final {
public:
    // The board has eight rows and columns, with one world unit per square.
    static constexpr std::int32_t Size = 8;

    /**
     * Creates the board squares and a transparent hover highlight.
     * Call once before updating the board.
     *
     * @param world The registry that owns the board's visual entities.
     */
    void Initialize(svanes::Registry &world);

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

    // The square under the mouse, or no value when the mouse is off the board.
    std::optional<BoardCoordinate> hoveredSquare;

    // A translucent rectangle moved over the hovered square. Its alpha is
    // zero when no board square is hovered.
    svanes::Entity hoverHighlight = 0;
};

} // namespace ghqcxx

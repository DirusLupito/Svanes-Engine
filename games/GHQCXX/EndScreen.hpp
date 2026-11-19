#pragma once

#include "Board.hpp"

#include <svanes/entity.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <optional>
#include <vector>

namespace svanes {
struct ClientFrameContext;
class Registry;
} // namespace svanes

namespace ghqcxx {

class BoardView;

/**
 * Presents the result of a finished game over the final board position.
 */
class EndScreen final {
public:
    /**
     * Starts the transition from the board to the result screen.
     *
     * @param world The client's registry.
     * @param result The outcome of the game.
     * @param transitionSeconds How long the black squares grow before the
     * result starts fading in. Zero skips the squares.
     *
     * @throws std::invalid_argument if the game is still in progress or
     * the duration cannot be represented in tics.
     */
    EndScreen(svanes::Registry &world, Board::Result result,
              double transitionSeconds = 6.0);

    /**
     * Advances the transition and the flashing result label.
     *
     * @param frame The current frame's time and rendering context.
     * @param view The board view that supplies the label's font.
     */
    void Update(const svanes::ClientFrameContext &frame, const BoardView &view);

private:
    // The black shape drawn over the board.
    svanes::Entity overlay;

    // The centered label naming the winning team or declaring a draw.
    svanes::Entity resultLabel;

    // The length of the square animation in tics.
    svanes::TicCount duration;

    // The client's clock for the end screen.
    svanes::Timeline timeline;

    // The tic when the result label first reached full brightness.
    std::optional<svanes::TicCount> pulseStart;

    // Square centers as fractions of the viewport, in spawn order.
    std::vector<svanes::Vector2D> centers;
};

} // namespace ghqcxx

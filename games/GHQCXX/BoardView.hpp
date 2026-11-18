#pragma once

#include "Board.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <array>
#include <variant>
#include <vector>

namespace svanes {
class Camera2D;
class InputManager;
class Registry;
} // namespace svanes

namespace ghqcxx {

// The client's current board or reserve selection.
using BoardSelection =
    std::variant<std::monostate, Board::BoardCoordinate, Piece>;

/**
 * Represents the client's interactive view of a GHQ board and its reserves.
 */
class BoardView final {
public:
    // The side length of a board square in world units.
    static constexpr float SquareSize = 1.4F;

    // The side length of the checkerboard in world units.
    static constexpr float BoardWidth = Board::Size * SquareSize;

    /**
     * Creates the board and reserve visuals from the current state. Call once
     * before updating the view.
     *
     * @param world The registry that owns the view's visual entities.
     * @param board The board whose current setup will be displayed.
     * @param fonts The manager used to load the reserve labels' font.
     * @param camera The camera used to position the labels.
     *
     * @throws std::logic_error if the view has already been initialized.
     * @throws std::runtime_error if the reserve font cannot be loaded.
     */
    void Initialize(svanes::Registry &world, const Board &board,
                    svanes::FontManager &fontManager,
                    const svanes::Camera2D &camera);

    /**
     * Synchronizes the displayed pieces and reserve counts with the board's
     * state. Only nonempty reserves are shown.
     *
     * @param world The registry used to initialize the view.
     * @param board The board whose pieces will be displayed.
     * @param camera The camera used to position the reserve labels.
     */
    void UpdatePieces(svanes::Registry &world, const Board &board,
                      const svanes::Camera2D &camera);

    /**
     * Updates mouse feedback and reports clicks on board squares and
     * available reserve pieces.
     *
     * @param world The registry used to initialize the view.
     * @param board The board containing the current reserves.
     * @param input The current frame's mouse position and button states.
     * @param camera The camera used to draw the board, already fitted to the
     * current window size.
     *
     * @return The square or reserve piece left-clicked this frame, or no
     * selection.
     */
    BoardSelection Update(svanes::Registry &world, const Board &board,
                          const svanes::InputManager &input,
                          const svanes::Camera2D &camera);

    /**
     * Displays selection feedback for a piece on the board or in reserve.
     *
     * @param world The registry used to initialize the view.
     * @param board The board used to find the selected piece's movement
     * options.
     * @param selection The client's current selection.
     * @param choosingFacing Whether the player is choosing the selected
     * artillery's facing.
     */
    void ShowSelection(svanes::Registry &world, const Board &board,
                       const BoardSelection &selection, bool choosingFacing);

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
    // Left-to-right order of the piece types displayed in each reserve row.
    static constexpr std::array ReserveTypes{
        PieceType::BasicArtillery,    PieceType::HeavyArtillery,
        PieceType::ArmoredArtillery,  PieceType::BasicInfantry,
        PieceType::ParachuteInfantry, PieceType::ArmoredInfantry};

    /**
     * Locates a reserve slot in world coordinates.
     *
     * @param team The team whose reserve row contains the slot.
     * @param index The slot's left-to-right index in that row.
     * @return The center of the slot.
     */
    static svanes::Vector2D ReserveCenter(Team team, std::size_t index);

    // The board and feedback entities are created once before view updates.
    bool initialized = false;

    // The pieces currently displayed, indexed by row, then column.
    std::array<std::array<std::optional<Piece>, Board::Size>, Board::Size>
        displayedPieces;

    // The visual entities belonging to each square's displayed piece.
    std::array<std::array<std::vector<svanes::Entity>, Board::Size>,
               Board::Size>
        pieceEntities;

    // Piece visuals and count labels for each team's available reserves.
    std::array<std::array<std::vector<svanes::Entity>, ReserveTypes.size()>,
               TeamCount>
        reserveEntities;

    // Font used by the reserve count labels, owned by the font manager.
    svanes::FontHandle reserveFont;

    // A translucent rectangle moved over the hovered square. Its alpha is
    // zero when no board square is hovered.
    svanes::Entity hoverHighlight = 0;

    // A highlight rectangle behind the selected piece.
    svanes::Entity selectionHighlight = 0;

    // The team-colored square behind the selected reserve piece's black
    // highlight.
    svanes::Entity reserveOutline = 0;

    // Circles marking selectable squares, indexed by row, then column.
    std::array<std::array<svanes::Entity, Board::Size>, Board::Size>
        movementMarkers{};
};

} // namespace ghqcxx

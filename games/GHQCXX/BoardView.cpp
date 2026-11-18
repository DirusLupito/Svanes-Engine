#include "BoardView.hpp"

#include "pieces/ArmoredArtillery.hpp"
#include "pieces/ArmoredInfantry.hpp"
#include "pieces/BasicArtillery.hpp"
#include "pieces/BasicInfantry.hpp"
#include "pieces/Headquarters.hpp"
#include "pieces/HeavyArtillery.hpp"
#include "pieces/ParachuteInfantry.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/geometry/circle_geometry.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/vector2d.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace ghqcxx {

// One-unit square shared by the board tiles and their highlights.
static constexpr svanes::Rectangle2D SquareGeometry{.width = 1.0F,
                                                    .height = 1.0F};

// Offset from a square's upper-left corner to its center on either axis.
static constexpr float SquareCenterOffset = SquareGeometry.width / 2.0F;

// Reserve row centers sit this far outside the top and bottom board edges.
static constexpr float ReserveRowOffset = 0.75F;

// World x coordinate of the first reserve piece's center.
static constexpr float ReserveStartX = 1.8F;

// Distance between reserve piece centers, in world units.
static constexpr float ReserveSpacing = 1.1F;

// Width and height of the team-colored square around a selected reserve piece.
static constexpr float ReserveOutlineSize = 1.08F;

// The label's bottom-left anchor relative to the piece's center, in world
// units.
static constexpr svanes::Vector2D ReserveLabelOffset{0.45F, 0.5F};

// Font size in points, independent of the camera's world scale.
static constexpr float ReserveFontSize = 24.0F;

// Size of each available-move indicator, in world units.
static constexpr float MovementMarkerRadius = 0.15F;

// Fill color of the light board squares.
static constexpr svanes::Color LightSquareColor{100, 102, 104};

// Fill color of the dark board squares.
static constexpr svanes::Color DarkSquareColor{36, 40, 44};

// Color and visible opacity of the highlight under the mouse.
static constexpr svanes::Color HoverColor{255, 255, 255, 80};

// Color and visible opacity of the selected square's highlight.
static constexpr svanes::Color SelectionColor{255, 210, 80, 96};

// Color and visible opacity of the available-move indicators.
static constexpr svanes::Color MovementMarkerColor{60, 200, 90, 220};

// Color used for Red's board and reserve pieces.
static constexpr svanes::Color NCStateRed{0xcc, 0x00, 0x00};

// Color used for Blue's board and reserve pieces.
static constexpr svanes::Color CarolinaBlue{0x7b, 0xaf, 0xd4};

/**
 * Locates a reserve slot in world coordinates.
 *
 * @param team The team whose reserve row contains the slot.
 * @param index The slot's left-to-right index in that row.
 * @return The center of the slot.
 */
static svanes::Vector2D ReserveCenter(Team team, std::size_t index) {
    return {ReserveStartX + static_cast<float>(index) * ReserveSpacing,
            team == Team::Red ? -ReserveRowOffset
                              : Board::Size + ReserveRowOffset};
}

/**
 * Creates a piece's visuals at a world position, facing the opposing
 * side.
 *
 * @param world The registry that owns the created entities.
 * @param piece The piece to display.
 * @param position The center of the piece in world coordinates.
 *
 * @return The entities making up the piece.
 *
 * @throws std::invalid_argument if the piece type or team is invalid.
 */
static std::vector<svanes::Entity>
CreatePieceVisual(svanes::Registry &world, Piece piece,
                  svanes::Vector2D position) {
    const auto color = BoardView::GetTeamColor(piece.team);
    const svanes::Transform transform{
        .x = position.x,
        .y = position.y,
        .rotation = piece.team == Team::Red ? std::numbers::pi_v<float> : 0.0F};

    switch (piece.type) {
    case PieceType::Headquarters:
        return CreateHeadquarters(world, color, transform);
    case PieceType::BasicInfantry:
        return CreateBasicInfantry(world, color, transform);
    case PieceType::ArmoredInfantry:
        return CreateArmoredInfantry(world, color, transform);
    case PieceType::ParachuteInfantry:
        return CreateParachuteInfantry(world, color, transform);
    case PieceType::BasicArtillery:
        return CreateBasicArtillery(world, color, transform);
    case PieceType::HeavyArtillery:
        return CreateHeavyArtillery(world, color, transform);
    case PieceType::ArmoredArtillery:
        return CreateArmoredArtillery(world, color, transform);
    default:
        throw std::invalid_argument("Unknown GHQ piece type.");
    }
}

void BoardView::Initialize(svanes::Registry &world, const Board &board,
                           svanes::FontManager &fonts,
                           const svanes::Camera2D &camera) {
    if (initialized) {
        throw std::logic_error(
            "The GHQ board view has already been initialized.");
    }

    reserveFont = fonts.LoadFont(
        svanes::AssetPath("assets/ghqcxx/fonts/consola.ttf"), ReserveFontSize);
    if (reserveFont.id == 0) {
        throw std::runtime_error("Could not load the GHQCXX reserve font.");
    }

    // Pieces draw at layer zero and above while the board and highlights draw
    // below zero so that pieces appear on top of the board and highlights.
    constexpr std::int32_t boardLayer = -3;
    constexpr std::int32_t hoverLayer = boardLayer + 1;
    constexpr std::int32_t selectionLayer = hoverLayer + 1;

    // Each square is a rectangle centered halfway into its row and column.
    // The board therefore occupies world coordinates (0, 0) through (8, 8).
    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            const auto square = world.CreateEntity();
            world.AddComponent<svanes::ZOrder>(square,
                                               svanes::ZOrder{boardLayer});
            world.AddComponent<svanes::Transform>(
                square,
                svanes::Transform{
                    .x = static_cast<float>(column) + SquareCenterOffset,
                    .y = static_cast<float>(row) + SquareCenterOffset});
            world.AddComponent<svanes::SolidShape>(
                square, svanes::SolidShape{.color = (row + column) % 2 == 0
                                                        ? LightSquareColor
                                                        : DarkSquareColor,
                                           .geometry = SquareGeometry});
        }
    }

    // The hover highlight starts transparent until the mouse is over the board.
    hoverHighlight = world.CreateEntity();
    world.AddComponent<svanes::ZOrder>(hoverHighlight,
                                       svanes::ZOrder{hoverLayer});
    world.AddComponent<svanes::Transform>(hoverHighlight);
    auto &hoverShape = world.AddComponent<svanes::SolidShape>(
        hoverHighlight,
        svanes::SolidShape{.color = HoverColor, .geometry = SquareGeometry});
    hoverShape.color.alpha = 0;

    selectionHighlight = world.CreateEntity();
    world.AddComponent<svanes::ZOrder>(selectionHighlight,
                                       svanes::ZOrder{selectionLayer});
    world.AddComponent<svanes::Transform>(selectionHighlight);
    auto &selectionShape = world.AddComponent<svanes::SolidShape>(
        selectionHighlight, svanes::SolidShape{.color = SelectionColor,
                                               .geometry = SquareGeometry});
    selectionShape.color.alpha = 0;

    reserveOutline = world.CreateEntity();
    world.AddComponent<svanes::ZOrder>(reserveOutline,
                                       svanes::ZOrder{selectionLayer - 1});
    world.AddComponent<svanes::Transform>(reserveOutline);
    world.AddComponent<svanes::SolidShape>(
        reserveOutline, svanes::SolidShape{.color = {0, 0, 0, 0},
                                           .geometry = svanes::Rectangle2D{
                                               .width = ReserveOutlineSize,
                                               .height = ReserveOutlineSize}});

    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            auto &marker = movementMarkers[row][column];
            marker = world.CreateEntity();
            world.AddComponent<svanes::Transform>(
                marker,
                svanes::Transform{
                    .x = static_cast<float>(column) + SquareCenterOffset,
                    .y = static_cast<float>(row) + SquareCenterOffset});
            auto &markerShape = world.AddComponent<svanes::SolidShape>(
                marker,
                svanes::SolidShape{.color = MovementMarkerColor,
                                   .geometry = svanes::Circle2D{
                                       .radius = MovementMarkerRadius}});
            markerShape.color.alpha = 0;
        }
    }

    UpdatePieces(world, board, camera);
    initialized = true;
}

void BoardView::UpdatePieces(svanes::Registry &world, const Board &board,
                             const svanes::Camera2D &camera) {
    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            const auto &piece = board.GetPiece({column, row});
            auto &displayed = displayedPieces[row][column];
            if (!piece && !displayed) {
                continue;
            }
            if (piece && displayed && piece->type == displayed->type &&
                piece->team == displayed->team) {
                continue;
            }

            // Rebuild only changed squares, including every part of a piece.
            auto &entities = pieceEntities[row][column];
            for (const auto entity : entities) {
                world.DestroyEntity(entity);
            }
            entities.clear();
            if (piece) {
                entities = CreatePieceVisual(
                    world, *piece,
                    {static_cast<float>(column) + SquareCenterOffset,
                     static_cast<float>(row) + SquareCenterOffset});
            }
            displayed = piece;
        }
    }

    for (const auto team : {Team::Red, Team::Blue}) {
        auto &reserve = reserveEntities[static_cast<std::size_t>(team)];
        for (std::size_t index = 0; index < ReserveTypes.size(); ++index) {
            const Piece piece{ReserveTypes[index], team};
            const auto count = board.GetReserveCount(piece);
            auto &entities = reserve[index];
            if (count == 0) {
                // Remove every part of the piece along with its count label.
                for (const auto entity : entities) {
                    world.DestroyEntity(entity);
                }
                entities.clear();
                continue;
            }

            const auto center = ReserveCenter(team, index);
            if (entities.empty()) {
                entities = CreatePieceVisual(world, piece, center);
                const auto label = world.CreateEntity();
                world.AddComponent<svanes::TextLabel>(
                    label, svanes::TextLabel{
                               .color = GetTeamColor(team),
                               .font = reserveFont,
                               .alignment = svanes::TextAlignment::BottomLeft});
                entities.push_back(label);
            }

            auto &label =
                world.GetComponent<svanes::TextLabel>(entities.back());
            // Labels use screen coordinates. Convert their world positions
            // each frame so they follow the pieces as the camera changes.
            const auto screen =
                camera.WorldToScreen({.x = center.x + ReserveLabelOffset.x,
                                      .y = center.y + ReserveLabelOffset.y});
            label.text = std::to_string(count);
            label.position = {screen.x, screen.y};
        }
    }
}

svanes::Color BoardView::GetTeamColor(Team team) {
    switch (team) {
    case Team::Red:
        return NCStateRed;
    case Team::Blue:
        return CarolinaBlue;
    default:
        throw std::invalid_argument("Unknown GHQ team.");
    }
}

BoardSelection BoardView::Update(svanes::Registry &world, const Board &board,
                                 const svanes::InputManager &input,
                                 const svanes::Camera2D &camera) {
    auto &highlight = world.GetComponent<svanes::SolidShape>(hoverHighlight);
    highlight.color.alpha = 0;

    if (camera.OutputWidth() <= 0 || camera.OutputHeight() <= 0) {
        return {};
    }
    const bool clicked = input.WasMouseButtonPressed(svanes::MouseButton::Left);
    const auto mouse = input.MousePosition();
    const auto worldMouse =
        camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});

    // Each square is one world unit wide and tall. Check the bounds before
    // converting to integers, since a negative fraction would become zero.
    // The right and bottom edges are outside the board.
    if (worldMouse.x >= 0.0F && worldMouse.x < Board::Size &&
        worldMouse.y >= 0.0F && worldMouse.y < Board::Size) {
        const Board::BoardCoordinate hoveredSquare{
            .column = static_cast<std::uint8_t>(worldMouse.x),
            .row = static_cast<std::uint8_t>(worldMouse.y)};

        auto &position = world.GetComponent<svanes::Transform>(hoverHighlight);
        position.x =
            static_cast<float>(hoveredSquare.column) + SquareCenterOffset;
        position.y = static_cast<float>(hoveredSquare.row) + SquareCenterOffset;
        highlight.color = HoverColor;
        if (clicked) {
            return hoveredSquare;
        }
        return {};
    }

    if (clicked) {
        const Team team = worldMouse.y < 0.0F ? Team::Red : Team::Blue;
        for (std::size_t index = 0; index < ReserveTypes.size(); ++index) {
            const auto center = ReserveCenter(team, index);
            const Piece piece{ReserveTypes[index], team};
            if (std::abs(worldMouse.x - center.x) <=
                    ReserveOutlineSize / 2.0F &&
                std::abs(worldMouse.y - center.y) <=
                    ReserveOutlineSize / 2.0F &&
                board.GetReserveCount(piece) > 0) {
                return piece;
            }
        }
    }
    return {};
}

void BoardView::ShowSelection(svanes::Registry &world, const Board &board,
                              const BoardSelection &selection) {
    // Reuse the circles for each selection, hiding the previous destinations.
    for (const auto &row : movementMarkers) {
        for (const auto marker : row) {
            world.GetComponent<svanes::SolidShape>(marker).color.alpha = 0;
        }
    }

    auto &highlight =
        world.GetComponent<svanes::SolidShape>(selectionHighlight);
    highlight.color.alpha = 0;
    auto &outline = world.GetComponent<svanes::SolidShape>(reserveOutline);
    outline.color.alpha = 0;
    auto &position = world.GetComponent<svanes::Transform>(selectionHighlight);
    if (const auto *selectedSquare =
            std::get_if<Board::BoardCoordinate>(&selection)) {
        position.x =
            static_cast<float>(selectedSquare->column) + SquareCenterOffset;
        position.y =
            static_cast<float>(selectedSquare->row) + SquareCenterOffset;
        highlight.color = SelectionColor;

        for (const auto destination :
             board.GetMovementOptions(*selectedSquare)) {
            const auto marker =
                movementMarkers[destination.row][destination.column];
            world.GetComponent<svanes::SolidShape>(marker).color =
                MovementMarkerColor;
        }
    } else if (const auto *piece = std::get_if<Piece>(&selection)) {
        const auto slot = std::ranges::find(ReserveTypes, piece->type);
        if (slot != ReserveTypes.end() && board.GetReserveCount(*piece) > 0) {
            const auto center =
                ReserveCenter(piece->team, slot - ReserveTypes.begin());
            position.x = center.x;
            position.y = center.y;
            highlight.color = {0, 0, 0, 255};
            world.GetComponent<svanes::Transform>(reserveOutline) = position;
            outline.color = GetTeamColor(piece->team);
        }
    }
}

} // namespace ghqcxx

#include "BoardView.hpp"

#include "ClientRole.hpp"
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
#include <svanes/geometry/composite_geometry.hpp>
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

// The rectangle shared by the board tiles and their highlights.
static constexpr svanes::Rectangle2D SquareGeometry{
    .width = BoardView::SquareSize, .height = BoardView::SquareSize};

/**
 * Locates the center of a row or column in world coordinates.
 *
 * @param index The row or column on the board.
 * @return The center along that axis.
 */
static constexpr float SquareCenter(std::uint8_t index) {
    return (index + 0.5F) * BoardView::SquareSize;
}

// Reserve row centers sit this far outside the top and bottom board edges.
static constexpr float ReserveRowOffset = 0.69F;

// The side length of the team-colored reserve square in world units.
static constexpr float ReserveOutlineSize = 1.24F;

// The side length of the black square inside the reserve outline, in world
// units.
static constexpr float ReserveInsetSize = ReserveOutlineSize - 0.08F;

// The horizontal offset of a reserve piece from its slot center, in world
// units.
static constexpr float ReservePieceOffsetX = -0.14F;

// The right and bottom edges of the reserve label, measured from the slot
// center.
static constexpr float ReserveLabelEdge = ReserveInsetSize / 2.0F - 0.04F;

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

// Color of the squares covered by a possible artillery facing.
static constexpr svanes::Color FireMarkerColor{255, 220, 40, 255};

// The opacity of a board piece that has used its action.
static constexpr std::uint8_t ExhaustedPieceAlpha = 80;

// The layer for Blue's fire hatching above the checkerboard.
static constexpr std::int32_t BlueHatchLayer = -4;

// The layer for Red's fire hatching where the two colors cross.
static constexpr std::int32_t RedHatchLayer = BlueHatchLayer + 1;

// Color used for Red's board and reserve pieces.
static constexpr svanes::Color NCStateRed{0xcc, 0x00, 0x00};

// Color used for Blue's board and reserve pieces.
static constexpr svanes::Color CarolinaBlue{0x7b, 0xaf, 0xd4};

svanes::Vector2D BoardView::ReserveCenter(Team team, std::size_t index) {
    constexpr float spacing = BoardWidth / ReserveTypes.size();
    return {(static_cast<float>(index) + 0.5F) * spacing,
            team == Team::Red ? -ReserveRowOffset
                              : BoardWidth + ReserveRowOffset};
}

/**
 * Creates the diagonal stripes that mark a square under artillery fire.
 *
 * @return The stripes centered on one board square.
 */
static svanes::CompositeShape2D CreateFireHatch() {
    constexpr float size = BoardView::SquareSize;
    constexpr float thickness = 0.035F * size;
    svanes::CompositeShape2D hatch;
    for (const float offset : {-0.375F, -0.125F, 0.125F, 0.375F}) {
        const float center = offset * size;
        // Shorten each diagonal by its thickness so its corners stay inside
        // the square.
        const float span = size - 2.0F * std::abs(center);
        hatch.parts.push_back(
            {.shape =
                 svanes::Rectangle2D{
                     .width = span * std::numbers::sqrt2_v<float> - thickness,
                     .height = thickness},
             .transform = {.x = center,
                           .y = center,
                           .rotation = -std::numbers::pi_v<float> / 4.0F}});
    }
    return hatch;
}

/**
 * Creates a piece's visuals at a world position using its board facing.
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
    constexpr float eighthTurn = std::numbers::pi_v<float> / 4.0F;
    const svanes::Transform transform{
        .x = position.x,
        .y = position.y,
        .rotation = static_cast<float>(piece.facing) * eighthTurn};

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
                           svanes::FontManager &fontManager,
                           const svanes::Camera2D &camera) {
    if (initialized) {
        throw std::logic_error(
            "The GHQ board view has already been initialized.");
    }

    const auto fontPath = svanes::AssetPath("assets/ghqcxx/fonts/consola.ttf");
    for (std::size_t i = 0; i < labelFonts.size(); ++i) {
        labelFonts[i] = fontManager.LoadFont(fontPath, LabelFontSizes[i]);
        if (labelFonts[i].id == 0) {
            throw std::runtime_error(
                "Could not load Consolas for the GHQCXX board labels.");
        }
    }

    turnLabel = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(turnLabel);
    ShowTurn(world, board.GetActiveTeam(), camera);

    roleHeading = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        roleHeading,
        svanes::TextLabel{.text = "Playing as:",
                          .color = {255, 255, 255},
                          .alignment = svanes::TextAlignment::TopRight,
                          .visible = false});
    roleLabel = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        roleLabel,
        svanes::TextLabel{.alignment = svanes::TextAlignment::TopRight});

    // Pieces draw at layer zero and above while the board and highlights draw
    // below zero so that pieces appear on top of the board and highlights.
    constexpr std::int32_t boardLayer = BlueHatchLayer - 1;
    constexpr std::int32_t hoverLayer = RedHatchLayer + 1;
    constexpr std::int32_t selectionLayer = hoverLayer + 1;

    // The marker layer above the piece decals on occupied squares.
    constexpr std::int32_t markerLayer = 5;

    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            const auto square = world.CreateEntity();
            world.AddComponent<svanes::ZOrder>(square,
                                               svanes::ZOrder{boardLayer});
            world.AddComponent<svanes::Transform>(
                square, svanes::Transform{.x = SquareCenter(column),
                                          .y = SquareCenter(row)});
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
            world.AddComponent<svanes::ZOrder>(marker,
                                               svanes::ZOrder{markerLayer});
            world.AddComponent<svanes::Transform>(
                marker, svanes::Transform{.x = SquareCenter(column),
                                          .y = SquareCenter(row)});
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
    static const auto fireHatch = CreateFireHatch();
    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            const auto &piece = board.GetPiece({column, row});
            auto &displayed = displayedPieces[row][column];
            if (piece == displayed) {
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
                    world, *piece, {SquareCenter(column), SquareCenter(row)});

                if (piece->action != ActionState::Ready) {
                    for (const auto entity : entities) {
                        world.GetComponent<svanes::SolidShape>(entity)
                            .color.alpha = ExhaustedPieceAlpha;
                    }
                }

                for (const auto &[square, facing] :
                     board.GetFireOptions({column, row})) {
                    if (facing != piece->facing) {
                        continue;
                    }
                    const bool red = piece->team == Team::Red;
                    const auto hatch = world.CreateEntity();
                    world.AddComponent<svanes::Transform>(
                        hatch,
                        svanes::Transform{
                            .x = SquareCenter(square.column),
                            .y = SquareCenter(square.row),
                            .rotation =
                                red ? std::numbers::pi_v<float> / 2.0F : 0.0F});
                    world.AddComponent<svanes::ZOrder>(
                        hatch,
                        svanes::ZOrder{red ? RedHatchLayer : BlueHatchLayer});
                    world.AddComponent<svanes::SolidShape>(
                        hatch,
                        svanes::SolidShape{.color = GetTeamColor(piece->team),
                                           .geometry = fireHatch});
                    entities.push_back(hatch);
                }
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
                entities = CreatePieceVisual(
                    world, piece, {center.x + ReservePieceOffsetX, center.y});
                const auto label = world.CreateEntity();
                world.AddComponent<svanes::TextLabel>(
                    label,
                    svanes::TextLabel{.color = GetTeamColor(team),
                                      .font = labelFonts.front(),
                                      .alignment =
                                          svanes::TextAlignment::BottomRight});
                entities.push_back(label);
            }

            auto &label =
                world.GetComponent<svanes::TextLabel>(entities.back());
            // Labels use screen coordinates. Convert their world positions
            // each frame so they follow the pieces as the camera changes.
            const auto screen =
                camera.WorldToScreen({.x = center.x + ReserveLabelEdge,
                                      .y = center.y + ReserveLabelEdge});
            label.text = std::to_string(count);
            label.position = {screen.x, screen.y};
        }
    }
}

void BoardView::ShowTurn(svanes::Registry &world, Team team,
                         const svanes::Camera2D &camera) {
    const auto topLeft = camera.WorldToScreen({});
    const float sideWidth = topLeft.x;
    const bool red = team == Team::Red;
    auto &label = world.GetComponent<svanes::TextLabel>(turnLabel);
    label.text = red ? "RED'S\nTURN" : "BLUE'S\nTURN";
    label.color = GetTeamColor(team);
    label.alignment = red ? svanes::TextAlignment::TopCenter
                          : svanes::TextAlignment::BottomCenter;
    label.position =
        red ? svanes::Vector2D{sideWidth * 0.5F, topLeft.y}
            : svanes::Vector2D{camera.OutputWidth() - sideWidth * 0.5F,
                               camera.OutputHeight() - topLeft.y};

    // Allow room for the six characters in BLUE'S and a little padding.
    std::size_t size = labelFonts.size() - 1;
    while (size > 0 && sideWidth < 4.0F * LabelFontSizes[size]) {
        --size;
    }

    label.font = labelFonts[size];
}

void BoardView::ShowClientRole(svanes::Registry &world,
                               std::optional<Team> team,
                               const svanes::Camera2D &camera) {
    // Use the smaller font when the heading would be wider than the side
    // margin.
    const float sideWidth = camera.WorldToScreen({}).x;
    const std::size_t size = sideWidth >= 6.5F * LabelFontSizes[1] ? 1 : 0;
    const float padding = LabelFontSizes[size] * 0.25F;
    auto &heading = world.GetComponent<svanes::TextLabel>(roleHeading);
    heading.visible = true;
    heading.font = labelFonts[size];
    heading.position = {camera.OutputWidth() - padding, padding};

    auto &label = world.GetComponent<svanes::TextLabel>(roleLabel);
    label.text = GetRoleName(team);
    label.color = team ? GetTeamColor(*team) : heading.color;
    label.font = heading.font;
    label.position = {heading.position.x,
                      heading.position.y + LabelFontSizes[size] + padding};
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

    // Check the bounds before converting to integers, since a negative
    // fraction would become zero.
    // The right and bottom edges are outside the board.
    if (worldMouse.x >= 0.0F && worldMouse.x < BoardWidth &&
        worldMouse.y >= 0.0F && worldMouse.y < BoardWidth) {
        const Board::BoardCoordinate hoveredSquare{
            .column = static_cast<std::uint8_t>(worldMouse.x / SquareSize),
            .row = static_cast<std::uint8_t>(worldMouse.y / SquareSize)};

        auto &position = world.GetComponent<svanes::Transform>(hoverHighlight);
        position.x = SquareCenter(hoveredSquare.column);
        position.y = SquareCenter(hoveredSquare.row);
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
                              const BoardSelection &selection,
                              bool choosingFacing) {
    const auto *reserve = std::get_if<Piece>(&selection);
    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            auto &marker = world.GetComponent<svanes::SolidShape>(
                movementMarkers[row][column]);
            if (reserve && board.CanPlaceFromReserve(*reserve, {column, row})) {
                marker.color = MovementMarkerColor;
            } else {
                marker.color.alpha = 0;
            }
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
        position.x = SquareCenter(selectedSquare->column);
        position.y = SquareCenter(selectedSquare->row);
        highlight.color = SelectionColor;
        highlight.geometry = SquareGeometry;

        if (choosingFacing) {
            for (const auto &[square, facing] :
                 board.GetFireOptions(*selectedSquare)) {
                const auto marker = movementMarkers[square.row][square.column];
                world.GetComponent<svanes::SolidShape>(marker).color =
                    FireMarkerColor;
            }
            return;
        }

        for (const auto destination :
             board.GetMovementOptions(*selectedSquare)) {
            const auto marker =
                movementMarkers[destination.row][destination.column];
            world.GetComponent<svanes::SolidShape>(marker).color =
                MovementMarkerColor;
        }
    } else if (reserve) {
        const auto slot = std::ranges::find(ReserveTypes, reserve->type);
        if (slot != ReserveTypes.end() && board.GetReserveCount(*reserve) > 0) {
            const auto center =
                ReserveCenter(reserve->team, slot - ReserveTypes.begin());
            position.x = center.x;
            position.y = center.y;
            highlight.color = {0, 0, 0, 255};
            highlight.geometry = svanes::Rectangle2D{
                .width = ReserveInsetSize, .height = ReserveInsetSize};
            world.GetComponent<svanes::Transform>(reserveOutline) = position;
            outline.color = GetTeamColor(reserve->team);
        }
    }
}

} // namespace ghqcxx

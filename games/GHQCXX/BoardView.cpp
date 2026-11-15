#include "BoardView.hpp"

#include "pieces/ArmoredArtillery.hpp"
#include "pieces/ArmoredInfantry.hpp"
#include "pieces/BasicArtillery.hpp"
#include "pieces/BasicInfantry.hpp"
#include "pieces/Headquarters.hpp"
#include "pieces/HeavyArtillery.hpp"
#include "pieces/ParachuteInfantry.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <iostream>
#include <numbers>
#include <stdexcept>

namespace ghqcxx {

/**
 * Creates a piece's visuals at the center of its square, facing the opposing
 * side.
 *
 * @param world The registry that owns the created entities.
 * @param piece The piece to display.
 * @param position The piece's square.
 *
 * @throws std::invalid_argument if the piece type or team is invalid.
 */
static void CreatePieceVisual(svanes::Registry &world, Board::Piece piece,
                              Board::BoardCoordinate position) {
    const auto color = BoardView::GetTeamColor(piece.team);
    const svanes::Transform transform{
        .x = static_cast<float>(position.column) + 0.5F,
        .y = static_cast<float>(position.row) + 0.5F,
        .rotation =
            piece.team == Board::Team::Red ? std::numbers::pi_v<float> : 0.0F};

    switch (piece.type) {
    case Board::PieceType::Headquarters:
        CreateHeadquarters(world, color, transform);
        break;
    case Board::PieceType::BasicInfantry:
        CreateBasicInfantry(world, color, transform);
        break;
    case Board::PieceType::ArmoredInfantry:
        CreateArmoredInfantry(world, color, transform);
        break;
    case Board::PieceType::ParachuteInfantry:
        CreateParachuteInfantry(world, color, transform);
        break;
    case Board::PieceType::BasicArtillery:
        CreateBasicArtillery(world, color, transform);
        break;
    case Board::PieceType::HeavyArtillery:
        CreateHeavyArtillery(world, color, transform);
        break;
    case Board::PieceType::ArmoredArtillery:
        CreateArmoredArtillery(world, color, transform);
        break;
    default:
        throw std::invalid_argument("Unknown GHQ piece type.");
    }
}

void BoardView::Initialize(svanes::Registry &world, const Board &board) {
    if (initialized) {
        throw std::logic_error(
            "The GHQ board view has already been initialized.");
    }

    constexpr svanes::Color light{100, 102, 104};
    constexpr svanes::Color dark{36, 40, 44};

    // Each square is a rectangle centered halfway into its row and column.
    // The board therefore occupies world coordinates (0, 0) through (8, 8).
    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            const auto square = world.CreateEntity();
            world.AddComponent<svanes::Transform>(
                square,
                svanes::Transform{.x = static_cast<float>(column) + 0.5F,
                                  .y = static_cast<float>(row) + 0.5F});
            world.AddComponent<svanes::SolidShape>(
                square, svanes::SolidShape{
                            .color = (row + column) % 2 == 0 ? light : dark,
                            .geometry = svanes::Rectangle2D{.width = 1.0F,
                                                            .height = 1.0F}});
        }
    }

    // Create the highlight after the board so it draws over the squares.
    // It starts transparent until the mouse is over the board.
    hoverHighlight = world.CreateEntity();
    world.AddComponent<svanes::Transform>(hoverHighlight);
    world.AddComponent<svanes::SolidShape>(
        hoverHighlight, svanes::SolidShape{.color = {255, 255, 255, 0},
                                           .geometry = svanes::Rectangle2D{
                                               .width = 1.0F, .height = 1.0F}});

    selectionHighlight = world.CreateEntity();
    world.AddComponent<svanes::Transform>(selectionHighlight);
    world.AddComponent<svanes::SolidShape>(
        selectionHighlight,
        svanes::SolidShape{
            .color = {255, 210, 80, 0},
            .geometry = svanes::Rectangle2D{.width = 1.0F, .height = 1.0F}});

    for (std::uint8_t row = 0; row < Board::Size; ++row) {
        for (std::uint8_t column = 0; column < Board::Size; ++column) {
            if (const auto &piece = board.GetPiece({column, row})) {
                CreatePieceVisual(world, *piece, {column, row});
            }
        }
    }

    initialized = true;
}

svanes::Color BoardView::GetTeamColor(Board::Team team) {
    switch (team) {
    case Board::Team::Red:
        return {0xcc, 0x00, 0x00};
    case Board::Team::Blue:
        return {0x7b, 0xaf, 0xd4};
    default:
        throw std::invalid_argument("Unknown GHQ team.");
    }
}

std::optional<Board::BoardCoordinate>
BoardView::Update(svanes::Registry &world, const svanes::InputManager &input,
                  const svanes::Camera2D &camera) {
    hoveredSquare.reset();
    auto &highlight = world.GetComponent<svanes::SolidShape>(hoverHighlight);
    highlight.color.alpha = 0;

    if (camera.OutputWidth() > 0 && camera.OutputHeight() > 0) {
        const auto mouse = input.MousePosition();
        const auto worldMouse =
            camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});

        // Each square is one world unit wide and tall. Check the bounds before
        // converting to integers, since a negative fraction would become zero.
        // The right and bottom edges are outside the board.
        if (worldMouse.x >= 0.0F && worldMouse.x < Board::Size &&
            worldMouse.y >= 0.0F && worldMouse.y < Board::Size) {
            hoveredSquare = Board::BoardCoordinate{
                .column = static_cast<std::uint8_t>(worldMouse.x),
                .row = static_cast<std::uint8_t>(worldMouse.y)};

            auto &position =
                world.GetComponent<svanes::Transform>(hoverHighlight);
            position.x = static_cast<float>(hoveredSquare->column) + 0.5F;
            position.y = static_cast<float>(hoveredSquare->row) + 0.5F;
            highlight.color.alpha = 80;
        }
    }

    if (hoveredSquare &&
        input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        std::cout << "Clicked board square: ("
                  << static_cast<std::uint32_t>(hoveredSquare->column) << ", "
                  << static_cast<std::uint32_t>(hoveredSquare->row) << ")\n"
                  << std::flush;
        return hoveredSquare;
    }
    return std::nullopt;
}

void BoardView::ShowSelection(
    svanes::Registry &world,
    const std::optional<Board::BoardCoordinate> &selectedSquare) {
    auto &highlight =
        world.GetComponent<svanes::SolidShape>(selectionHighlight);
    highlight.color.alpha = 0;
    if (selectedSquare) {
        auto &position =
            world.GetComponent<svanes::Transform>(selectionHighlight);
        position.x = static_cast<float>(selectedSquare->column) + 0.5F;
        position.y = static_cast<float>(selectedSquare->row) + 0.5F;
        highlight.color.alpha = 96;
    }
}

} // namespace ghqcxx

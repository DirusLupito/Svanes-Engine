#include "Board.hpp"

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

void Board::Initialize(svanes::Registry &world) {
    if (initialized) {
        throw std::logic_error("The GHQ board has already been initialized.");
    }

    constexpr svanes::Color light{200, 204, 208};
    constexpr svanes::Color dark{72, 80, 88};

    // Each square is a rectangle centered halfway into its row and column.
    // The board therefore occupies world coordinates (0, 0) through (8, 8).
    for (std::uint8_t row = 0; row < Size; ++row) {
        for (std::uint8_t column = 0; column < Size; ++column) {
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

    initialized = true;

    // Starting according to the rulebook.
    PlacePiece(world, {PieceType::Headquarters, Team::Red}, {0, 0});
    PlacePiece(world, {PieceType::Headquarters, Team::Blue}, {7, 7});
    PlacePiece(world, {PieceType::BasicArtillery, Team::Red}, {1, 0});
    PlacePiece(world, {PieceType::BasicArtillery, Team::Blue}, {6, 7});

    for (std::uint8_t column = 0; column < 3; ++column) {
        PlacePiece(world, {PieceType::BasicInfantry, Team::Red}, {column, 1});
        PlacePiece(world, {PieceType::BasicInfantry, Team::Blue},
                   {static_cast<std::uint8_t>(Size - 1 - column), 6});
    }
}

void Board::PlacePiece(svanes::Registry &world, Piece piece,
                       BoardCoordinate position) {
    if (!initialized) {
        throw std::logic_error(
            "Initialize the GHQ board before placing pieces.");
    }
    if (GetPiece(position)) {
        throw std::logic_error(
            "Cannot place a piece on an occupied GHQ square.");
    }

    const auto color = GetTeamColor(piece.team);
    const svanes::Transform transform{
        .x = static_cast<float>(position.column) + 0.5F,
        .y = static_cast<float>(position.row) + 0.5F,
        .rotation = piece.team == Team::Red ? std::numbers::pi_v<float> : 0.0F};

    switch (piece.type) {
    case PieceType::Headquarters:
        CreateHeadquarters(world, color, transform);
        break;
    case PieceType::BasicInfantry:
        CreateBasicInfantry(world, color, transform);
        break;
    case PieceType::ArmoredInfantry:
        CreateArmoredInfantry(world, color, transform);
        break;
    case PieceType::ParachuteInfantry:
        CreateParachuteInfantry(world, color, transform);
        break;
    case PieceType::BasicArtillery:
        CreateBasicArtillery(world, color, transform);
        break;
    case PieceType::HeavyArtillery:
        CreateHeavyArtillery(world, color, transform);
        break;
    case PieceType::ArmoredArtillery:
        CreateArmoredArtillery(world, color, transform);
        break;
    default:
        throw std::invalid_argument("Unknown GHQ piece type.");
    }

    pieces[position.row][position.column] = piece;
}

const std::optional<Board::Piece> &
Board::GetPiece(BoardCoordinate position) const {
    if (position.column >= Size || position.row >= Size) {
        throw std::out_of_range(
            "GHQ board coordinates must be between 0 and 7.");
    }
    return pieces[position.row][position.column];
}

svanes::Color Board::GetTeamColor(Team team) {
    switch (team) {
    case Team::Red:
        return {0xcc, 0x00, 0x00};
    case Team::Blue:
        return {0x7b, 0xaf, 0xd4};
    default:
        throw std::invalid_argument("Unknown GHQ team.");
    }
}

void Board::Update(svanes::Registry &world, const svanes::InputManager &input,
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
        if (worldMouse.x >= 0.0F && worldMouse.x < Size &&
            worldMouse.y >= 0.0F && worldMouse.y < Size) {
            hoveredSquare = BoardCoordinate{
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
    }
}

} // namespace ghqcxx

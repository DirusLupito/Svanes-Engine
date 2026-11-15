#include "ghqcxx_client.hpp"

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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace ghqcxx {

// The board has eight rows and columns, with one world unit per square.
constexpr std::int32_t BoardSize = 8;

// Leave two rows below the board for the red and blue piece visuals.
constexpr float ViewHeight = BoardSize + 2.75F;

/**
 * Fits the board and the pieces below it into the window. Both dimensions use
 * the same scale so the board's squares stay square when the window is resized.
 *
 * @param camera The client's camera, with its current rendering output size.
 */
static void FitBoardToWindow(svanes::Camera2D &camera) {
    // A minimized window may have no rendering area. Keep the previous zoom
    // until there is room to draw again, since a zero zoom is invalid.
    if (camera.OutputWidth() > 0 && camera.OutputHeight() > 0) {
        camera.zoom =
            std::min(static_cast<float>(camera.OutputWidth()) / BoardSize,
                     static_cast<float>(camera.OutputHeight()) / ViewHeight);
    }
}

void GHQCXXClient::Initialize(svanes::ClientContext &context) {
    constexpr svanes::Color light{200, 204, 208};
    constexpr svanes::Color dark{72, 80, 88};
    constexpr svanes::Color carolinaBlue{0x7b, 0xaf, 0xd4};
    constexpr svanes::Color ncStateRed{0xcc, 0x00, 0x00};

    // Each square is a rectangle centered halfway into its row and column.
    // The board therefore occupies world coordinates (0, 0) through (8, 8).
    for (std::int32_t row = 0; row < BoardSize; ++row) {
        for (std::int32_t column = 0; column < BoardSize; ++column) {
            const auto square = context.world.CreateEntity();
            context.world.AddComponent<svanes::Transform>(
                square,
                svanes::Transform{.x = static_cast<float>(column) + 0.5F,
                                  .y = static_cast<float>(row) + 0.5F});
            context.world.AddComponent<svanes::SolidShape>(
                square, svanes::SolidShape{
                            .color = (row + column) % 2 == 0 ? light : dark,
                            .geometry = svanes::Rectangle2D{.width = 1.0F,
                                                            .height = 1.0F}});
        }
    }

    // Create the highlight after the board so it draws over the squares.
    // It starts transparent until the mouse is over the board.
    hoverHighlight = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(hoverHighlight);
    context.world.AddComponent<svanes::SolidShape>(
        hoverHighlight, svanes::SolidShape{.color = {255, 255, 255, 0},
                                           .geometry = svanes::Rectangle2D{
                                               .width = 1.0F, .height = 1.0F}});

    CreateHeadquarters(context.world, ncStateRed,
                       {.x = 0.7F, .y = BoardSize + 0.75F});
    CreateHeadquarters(context.world, carolinaBlue,
                       {.x = 0.7F, .y = BoardSize + 2.0F});

    CreateBasicArtillery(context.world, ncStateRed,
                         {.x = 1.8F, .y = BoardSize + 0.75F});
    CreateBasicArtillery(context.world, carolinaBlue,
                         {.x = 1.8F, .y = BoardSize + 2.0F});

    CreateHeavyArtillery(context.world, ncStateRed,
                         {.x = 2.9F, .y = BoardSize + 0.75F});
    CreateHeavyArtillery(context.world, carolinaBlue,
                         {.x = 2.9F, .y = BoardSize + 2.0F});

    CreateArmoredArtillery(context.world, ncStateRed,
                           {.x = 4.0F, .y = BoardSize + 0.75F});
    CreateArmoredArtillery(context.world, carolinaBlue,
                           {.x = 4.0F, .y = BoardSize + 2.0F});

    CreateBasicInfantry(context.world, ncStateRed,
                        {.x = 5.1F, .y = BoardSize + 0.75F});
    CreateBasicInfantry(context.world, carolinaBlue,
                        {.x = 5.1F, .y = BoardSize + 2.0F});

    CreateParachuteInfantry(context.world, ncStateRed,
                            {.x = 6.2F, .y = BoardSize + 0.75F});
    CreateParachuteInfantry(context.world, carolinaBlue,
                            {.x = 6.2F, .y = BoardSize + 2.0F});

    CreateArmoredInfantry(context.world, ncStateRed,
                          {.x = 7.3F, .y = BoardSize + 0.75F});
    CreateArmoredInfantry(context.world, carolinaBlue,
                          {.x = 7.3F, .y = BoardSize + 2.0F});

    context.camera.x = BoardSize * 0.5F;
    context.camera.y = ViewHeight * 0.5F;
    context.camera.scale_mode = svanes::ScaleMode::Constant;
    FitBoardToWindow(context.camera);

    std::cout << "Hello, world! GHQCXX client.\n" << std::flush;
}

svanes::NetworkMessage GHQCXXClient::CreateJoinRequest() { return {}; }

void GHQCXXClient::HandleJoinResult(const svanes::JoinResult &result) {
    switch (result.status) {
    case svanes::JoinStatus::Accepted:
        std::cout << "Connected to GHQCXX as client " << result.client_id
                  << ".\n"
                  << std::flush;
        break;
    case svanes::JoinStatus::Rejected:
        throw std::runtime_error("GHQCXX server rejected the join request.");
    case svanes::JoinStatus::TimedOut:
        throw std::runtime_error(
            "GHQCXX server did not answer the join request.");
    }
}

void GHQCXXClient::HandleDisconnect() {
    throw std::runtime_error("Lost connection to the GHQCXX server.");
}

void GHQCXXClient::Update(const svanes::ClientFrameContext &context) {
    FitBoardToWindow(context.camera);

    hoveredSquare.reset();
    auto &highlight =
        context.world.GetComponent<svanes::SolidShape>(hoverHighlight);
    highlight.color.alpha = 0;

    if (context.camera.OutputWidth() > 0 && context.camera.OutputHeight() > 0) {
        const auto mouse = context.input.MousePosition();
        const auto worldMouse =
            context.camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});

        // Each square is one world unit wide and tall. Check the bounds before
        // converting to integers, since a negative fraction would become zero.
        // The right and bottom edges are outside the board.
        if (worldMouse.x >= 0.0F && worldMouse.x < BoardSize &&
            worldMouse.y >= 0.0F && worldMouse.y < BoardSize) {
            hoveredSquare = BoardCoordinate{
                .column = static_cast<std::int32_t>(worldMouse.x),
                .row = static_cast<std::int32_t>(worldMouse.y)};

            auto &position =
                context.world.GetComponent<svanes::Transform>(hoverHighlight);
            position.x = static_cast<float>(hoveredSquare->column) + 0.5F;
            position.y = static_cast<float>(hoveredSquare->row) + 0.5F;
            highlight.color.alpha = 80;
        }
    }

    if (hoveredSquare &&
        context.input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        std::cout << "Clicked board square: (" << hoveredSquare->column << ", "
                  << hoveredSquare->row << ")\n"
                  << std::flush;
    }

    if (context.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

bool GHQCXXClient::ShouldQuit() const { return quit; }

svanes::TicCount GHQCXXClient::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace ghqcxx

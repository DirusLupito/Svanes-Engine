#include "ghqcxx_client.hpp"

#include "BasicArtillery.hpp"
#include "HeavyArtillery.hpp"
#include "ParachuteInfantry.hpp"
#include "PieceVisuals.hpp"

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

// Leave a row below the board for the piece visuals.
constexpr float ViewHeight = BoardSize + 1.5F;

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

    const auto red_star = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        red_star, svanes::Transform{.x = 0.5F, .y = BoardSize + 0.75F});
    context.world.AddComponent<svanes::SolidShape>(
        red_star, CreateStarVisual({0xff, 0x7c, 0x7c}));

    const auto blue_star = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        blue_star, svanes::Transform{.x = 4.5F, .y = BoardSize + 0.75F});
    context.world.AddComponent<svanes::SolidShape>(
        blue_star, CreateStarVisual({0x7c, 0xb7, 0xff}));

    CreateBasicArtillery(context.world, {0xff, 0x7c, 0x7c},
                         {.x = 1.5F, .y = BoardSize + 0.75F});
    CreateBasicArtillery(context.world, {0x7c, 0xb7, 0xff},
                         {.x = 5.5F, .y = BoardSize + 0.75F});

    CreateHeavyArtillery(context.world, {0xff, 0x7c, 0x7c},
                         {.x = 2.5F, .y = BoardSize + 0.75F});
    CreateHeavyArtillery(context.world, {0x7c, 0xb7, 0xff},
                         {.x = 6.5F, .y = BoardSize + 0.75F});

    CreateParachuteInfantry(context.world, {0xff, 0x7c, 0x7c},
                            {.x = 3.5F, .y = BoardSize + 0.75F});
    CreateParachuteInfantry(context.world, {0x7c, 0xb7, 0xff},
                            {.x = 7.5F, .y = BoardSize + 0.75F});

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

    if (context.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

bool GHQCXXClient::ShouldQuit() const { return quit; }

svanes::TicCount GHQCXXClient::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace ghqcxx

#include "ghqcxx_client.hpp"

#include "events/ClientPacketReceivedEvent.hpp"
#include "events/MovementEvent.hpp"
#include "handlers/ClientPacketReceivedHandler.hpp"
#include "handlers/MovementHandler.hpp"
#include "pieces/ArmoredArtillery.hpp"
#include "pieces/ArmoredInfantry.hpp"
#include "pieces/BasicArtillery.hpp"
#include "pieces/BasicInfantry.hpp"
#include "pieces/HeavyArtillery.hpp"
#include "pieces/ParachuteInfantry.hpp"

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace ghqcxx {

// Leave a row above and below the board for the red and blue piece visuals.
constexpr float ViewHeight = Board::Size + 2.75F;

/**
 * Fits the board and both rows of pieces into the window. Both dimensions use
 * the same scale so the board's squares stay square when the window is resized.
 *
 * @param camera The client's camera, with its current rendering output size.
 */
static void FitViewToWindow(svanes::Camera2D &camera) {
    // A minimized window may have no rendering area. Keep the previous zoom
    // until there is room to draw again, since a zero zoom is invalid.
    if (camera.OutputWidth() > 0 && camera.OutputHeight() > 0) {
        camera.zoom =
            std::min(static_cast<float>(camera.OutputWidth()) / Board::Size,
                     static_cast<float>(camera.OutputHeight()) / ViewHeight);
    }
}

void GHQCXXClient::Initialize(svanes::ClientContext &context) {
    const auto carolinaBlue = BoardView::GetTeamColor(Team::Blue);
    const auto ncStateRed = BoardView::GetTeamColor(Team::Red);

    board.Initialize();
    boardView.Initialize(context.world, board);

    CreateBasicArtillery(
        context.world, ncStateRed,
        {.x = 1.8F, .y = -0.75F, .rotation = std::numbers::pi_v<float>});
    CreateBasicArtillery(context.world, carolinaBlue,
                         {.x = 1.8F, .y = Board::Size + 0.75F});

    CreateHeavyArtillery(
        context.world, ncStateRed,
        {.x = 2.9F, .y = -0.75F, .rotation = std::numbers::pi_v<float>});
    CreateHeavyArtillery(context.world, carolinaBlue,
                         {.x = 2.9F, .y = Board::Size + 0.75F});

    CreateArmoredArtillery(
        context.world, ncStateRed,
        {.x = 4.0F, .y = -0.75F, .rotation = std::numbers::pi_v<float>});
    CreateArmoredArtillery(context.world, carolinaBlue,
                           {.x = 4.0F, .y = Board::Size + 0.75F});

    CreateBasicInfantry(
        context.world, ncStateRed,
        {.x = 5.1F, .y = -0.75F, .rotation = std::numbers::pi_v<float>});
    CreateBasicInfantry(context.world, carolinaBlue,
                        {.x = 5.1F, .y = Board::Size + 0.75F});

    CreateParachuteInfantry(
        context.world, ncStateRed,
        {.x = 6.2F, .y = -0.75F, .rotation = std::numbers::pi_v<float>});
    CreateParachuteInfantry(context.world, carolinaBlue,
                            {.x = 6.2F, .y = Board::Size + 0.75F});

    CreateArmoredInfantry(
        context.world, ncStateRed,
        {.x = 7.3F, .y = -0.75F, .rotation = std::numbers::pi_v<float>});
    CreateArmoredInfantry(context.world, carolinaBlue,
                          {.x = 7.3F, .y = Board::Size + 0.75F});

    const auto reserveFont = context.fonts.LoadFont(
        svanes::AssetPath("assets/ghqcxx/fonts/consola.ttf"), 24.0F);
    if (reserveFont.id == 0) {
        throw std::runtime_error("Could not load the GHQCXX reserve font.");
    }

    for (const auto team : {Team::Red, Team::Blue}) {
        const auto color = BoardView::GetTeamColor(team);
        const float y = team == Team::Red ? -0.75F : Board::Size + 0.75F;
        for (const auto &[x, count] :
             {std::pair{1.8F, "2"}, std::pair{2.9F, "1"}, std::pair{4.0F, "1"},
              std::pair{5.1F, "5"}, std::pair{6.2F, "1"},
              std::pair{7.3F, "3"}}) {
            const auto label = context.world.CreateEntity();
            context.world.AddComponent<svanes::Transform>(
                label, svanes::Transform{.x = x + 0.45F, .y = y + 0.5F});
            context.world.AddComponent<svanes::TextLabel>(
                label, svanes::TextLabel{
                           .text = count,
                           .color = color,
                           .font = reserveFont,
                           .alignment = svanes::TextAlignment::BottomLeft});
        }
    }

    context.camera.x = Board::Size * 0.5F;
    context.camera.y = Board::Size * 0.5F;
    context.camera.scale_mode = svanes::ScaleMode::Constant;
    FitViewToWindow(context.camera);

    context.events.Subscribe(
        std::make_unique<ClientPacketReceivedHandler>(board, selectedPiece));
    context.events.Subscribe(
        std::make_unique<MovementHandler>(board, context.network));

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
    // Trigger immediately so handlers can update the board before it is drawn.
    for (const auto &message : context.network.PollMessages()) {
        context.events.TriggerEvent(ClientPacketReceivedEvent(message));
    }

    FitViewToWindow(context.camera);
    const auto clickedSquare =
        boardView.Update(context.world, context.input, context.camera);
    if (clickedSquare) {
        if (board.GetPiece(*clickedSquare)) {
            selectedPiece = clickedSquare;
        } else if (selectedPiece) {
            for (const auto destination :
                 board.GetMovementOptions(*selectedPiece)) {
                if (destination.column == clickedSquare->column &&
                    destination.row == clickedSquare->row) {
                    context.events.TriggerEvent(
                        MovementEvent(*selectedPiece, *clickedSquare));
                    selectedPiece = clickedSquare;
                    break;
                }
            }
        }
    }
    boardView.UpdatePieces(context.world, board);
    boardView.ShowSelection(context.world, board, selectedPiece);

    context.world.ForEach<svanes::Transform, svanes::TextLabel>(
        [&](svanes::Entity, const svanes::Transform &position,
            svanes::TextLabel &label) {
            const auto screen = context.camera.WorldToScreen(
                {.x = position.x, .y = position.y});
            label.position = {screen.x, screen.y};
        });

    if (context.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

bool GHQCXXClient::ShouldQuit() const { return quit; }

svanes::TicCount GHQCXXClient::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace ghqcxx

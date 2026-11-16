#include "ghqcxx_client.hpp"

#include "events/ClientPacketReceivedEvent.hpp"
#include "pieces/ArmoredArtillery.hpp"
#include "pieces/ArmoredInfantry.hpp"
#include "pieces/BasicArtillery.hpp"
#include "pieces/BasicInfantry.hpp"
#include "pieces/HeavyArtillery.hpp"
#include "pieces/ParachuteInfantry.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/network_client.hpp>

#include <algorithm>
#include <iostream>
#include <numbers>
#include <stdexcept>

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

    context.camera.x = Board::Size * 0.5F;
    context.camera.y = Board::Size * 0.5F;
    context.camera.scale_mode = svanes::ScaleMode::Constant;
    FitViewToWindow(context.camera);

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
    if (clickedSquare && board.GetPiece(*clickedSquare)) {
        selectedPiece = clickedSquare;
    }
    boardView.ShowSelection(context.world, board, selectedPiece);

    if (context.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

bool GHQCXXClient::ShouldQuit() const { return quit; }

svanes::TicCount GHQCXXClient::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace ghqcxx

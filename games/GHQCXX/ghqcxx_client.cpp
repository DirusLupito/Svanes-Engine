#include "ghqcxx_client.hpp"

#include "events/ClientPacketReceivedEvent.hpp"
#include "events/MovementEvent.hpp"
#include "events/PlacementEvent.hpp"
#include "handlers/ClientPacketReceivedHandler.hpp"
#include "handlers/EndTurnHandler.hpp"
#include "handlers/MovementHandler.hpp"
#include "handlers/PlacementHandler.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/network/clientserver/network_client.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace ghqcxx {

// Leave a row above and below the board for the red and blue piece visuals.
constexpr float ViewHeight = BoardView::BoardWidth + 2.75F;

// The board width including two squares of space on either side for the turn
// label.
constexpr float ViewWidth =
    BoardView::BoardWidth + 4.0F * BoardView::SquareSize;

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
            std::min(static_cast<float>(camera.OutputWidth()) / ViewWidth,
                     static_cast<float>(camera.OutputHeight()) / ViewHeight);
    }
}

void GHQCXXClient::Initialize(svanes::ClientContext &context) {
    board.Initialize();

    context.camera.x = BoardView::BoardWidth * 0.5F;
    context.camera.y = BoardView::BoardWidth * 0.5F;
    context.camera.scale_mode = svanes::ScaleMode::Constant;
    FitViewToWindow(context.camera);
    boardView.Initialize(context.world, board, context.fonts, context.camera);

    context.events.Subscribe(std::make_unique<ClientPacketReceivedHandler>(
        board, selection, choosingFacing));
    context.events.Subscribe(
        std::make_unique<MovementHandler>(board, context.network));
    context.events.Subscribe(
        std::make_unique<PlacementHandler>(board, context.network));
    context.events.Subscribe(std::make_unique<EndTurnHandler>(context.network));

    std::cout << "Hello, world! GHQCXX client.\n" << std::flush;
}

svanes::NetworkMessage GHQCXXClient::CreateJoinRequest() { return {}; }

void GHQCXXClient::HandleJoinResult(const svanes::JoinResult &result) {
    switch (result.status) {
    case svanes::JoinStatus::Accepted:
        board = Board::FromMessage(result.message);
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
    if (context.network.IsConnected() &&
        context.input.WasPressed(svanes::Key::D)) {
        context.events.TriggerEvent(EndTurnEvent{});
    }

    const auto clicked =
        boardView.Update(context.world, board, context.input, context.camera);
    if (const auto *clickedSquare =
            std::get_if<Board::BoardCoordinate>(&clicked)) {
        ClickSquare(*clickedSquare, context.events);
    } else if (const auto *reserve = std::get_if<Piece>(&clicked)) {
        selection = *reserve;
        choosingFacing = false;
    }

    boardView.UpdatePieces(context.world, board, context.camera);
    boardView.ShowTurn(context.world, board.GetActiveTeam(), context.camera);
    boardView.ShowSelection(context.world, board, selection, choosingFacing);

    if (context.input.WasPressed(svanes::Key::Escape)) {
        quit = true;
    }
}

void GHQCXXClient::ClickSquare(Board::BoardCoordinate square,
                               svanes::EventManager &events) {
    const auto *selected = std::get_if<Board::BoardCoordinate>(&selection);
    if (selected && choosingFacing) {
        if (square == *selected) {
            selection = std::monostate{};
            choosingFacing = false;
            return;
        }
        for (const auto &[covered, facing] : board.GetFireOptions(*selected)) {
            if (square == covered) {
                events.TriggerEvent(
                    MovementEvent(*selected, *selected, facing));
                selection = std::monostate{};
                choosingFacing = false;
                break;
            }
        }
        return;
    }

    if (const auto &piece = board.GetPiece(square)) {
        choosingFacing =
            selected && *selected == square && piece->GetFireRange() > 0;
        selection = square;
    } else if (const auto *reserve = std::get_if<Piece>(&selection)) {
        if (board.CanPlaceFromReserve(*reserve, square)) {
            events.TriggerEvent(PlacementEvent(*reserve, square));
            selection = std::monostate{};
        }
    } else if (selected && board.CanMovePiece(*selected, square)) {
        events.TriggerEvent(MovementEvent(*selected, square));
        choosingFacing = board.GetPiece(square)->GetFireRange() > 0;
        if (choosingFacing) {
            selection = square;
        } else {
            selection = std::monostate{};
        }
    }
}

bool GHQCXXClient::ShouldQuit() const { return quit; }

svanes::TicCount GHQCXXClient::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

} // namespace ghqcxx

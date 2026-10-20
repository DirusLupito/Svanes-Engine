#include "orbital_escalation_client.hpp"
#include "serialization/world_update.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <utility>

const svanes::TicCount kPauseFlashPeriod = svanes::SecondsToTics(1.0);
constexpr std::uint8_t kPauseLabelMinimumAlpha = 64;
constexpr svanes::TicCount kClientFrameTics = 16666;

OrbitalEscalationClient::OrbitalEscalationClient(std::string server_address)
    : server_address(std::move(server_address)) {
    if (this->server_address.empty()) {
        throw std::invalid_argument("The server address must be provided.");
    }
}

void OrbitalEscalationClient::Initialize(svanes::ClientContext &context) {
    context.network.Connect(server_address);
    std::cout << "Joining " << server_address << '.' << std::endl;

    pause_label_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::TextLabel>(
        pause_label_entity,
        svanes::TextLabel{
            .text = "PAUSED",
            .position = {context.camera.Viewport().width * 0.5F, 16.0F},
            .color = {255, 0, 0, 255},
            .font = context.fonts.LoadFont(
                svanes::AssetPath("assets/orbitalEscalation/fonts/consola.ttf"),
                24.0F),
            .alignment = svanes::TextAlignment::TopCenter,
            .visible = false,
        });

    context.camera.zoom = 0.02F;

    const svanes::Rectangle2D view =
        context.camera.ScreenToWorld(context.camera.Viewport());

    background_entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(
        background_entity, svanes::Transform{view.x, view.y});
    context.world.AddComponent<svanes::SolidShape>(
        background_entity,
        svanes::SolidShape{
            svanes::Color{0, 0, 68, 255},
            svanes::Rectangle2D{0.0F, 0.0F, view.width, view.height}});
    context.world.AddComponent<svanes::ZOrder>(background_entity,
                                               svanes::ZOrder{-100});
}

svanes::NetworkMessage OrbitalEscalationClient::CreateJoinRequest() {
    return {};
}

void OrbitalEscalationClient::HandleJoinResult(
    const svanes::JoinResult &result) {
    switch (result.status) {
    case svanes::JoinStatus::Accepted:
        std::cout << "Connected as player " << result.client_id << '.'
                  << std::endl;
        break;
    case svanes::JoinStatus::Rejected:
        std::cout << "Server rejected the join request.\n";
        break;
    case svanes::JoinStatus::TimedOut:
        std::cout << "Server join request timed out.\n";
        break;
    }
}

svanes::TicCount OrbitalEscalationClient::GetFrameIntervalTics() const {
    const auto interval =
        svanes::RationalNumber{kClientFrameTics} /
        (svanes::RationalNumber{1} / client_loop_timeline.GetTicSize());

    return interval.GetWholePart() +
           (interval.GetFractionalPart().GetNumerator() != 0);
}

void OrbitalEscalationClient::Update(const svanes::ClientFrameContext &frame) {
    // temporary debugging output of the world update messages received from the
    // server
    // TODO: remove this and replace it with an actual reader that turns server
    // messages into game state updates.
    for (const auto &message : frame.network.PollMessages()) {
        const auto update = WorldUpdate::Read(message);
        std::cout << "World update (" << message.bytes.size() << " bytes, "
                  << update.objects.size() << " objects): ";
        std::cout << std::hex << std::setfill('0');
        for (const auto byte : message.bytes) {
            std::cout << std::setw(2) << std::to_integer<std::uint32_t>(byte)
                      << ' ';
        }
        std::cout << std::dec << std::setfill(' ') << std::endl;
    }

    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    // +/- will speed up or slow down the client loop.
    const bool faster = frame.input.WasPressed(svanes::Key::Equals) ||
                        frame.input.WasPressed(svanes::Key::KeypadPlus);
    const bool slower = frame.input.WasPressed(svanes::Key::Minus) ||
                        frame.input.WasPressed(svanes::Key::KeypadMinus);
    if (faster != slower) {
        client_loop_timeline.SetTicSize(client_loop_timeline.GetTicSize() /
                                        (faster
                                             ? svanes::RationalNumber{2}
                                             : svanes::RationalNumber{1, 2}));
        std::cout << "Client loop rate: "
                  << static_cast<double>(svanes::TicsPerSecond) /
                         GetFrameIntervalTics()
                  << " updates/second requested.\n";
    }

    // The pause label's animation follows local time.
    pause_timeline.Advance(frame.real_delta_tics);
    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.camera.scale_mode =
            frame.camera.scale_mode == svanes::ScaleMode::Constant
                ? svanes::ScaleMode::Proportional
                : svanes::ScaleMode::Constant;
    }

    auto &pause_label =
        frame.world.GetComponent<svanes::TextLabel>(pause_label_entity);

    // I use a cosine wave to make the pause label flash while the game is
    // paused.
    const svanes::TicCount pause_tics =
        pause_timeline.GetTotalTics() % kPauseFlashPeriod;

    const float pause_flash_phase = 2.0F * std::numbers::pi_v<float> *
                                    static_cast<float>(pause_tics) /
                                    static_cast<float>(kPauseFlashPeriod);

    const float pause_flash_amount =
        0.5F * (1.0F + std::cos(pause_flash_phase));

    pause_label.color.alpha = static_cast<std::uint8_t>(
        std::lerp(static_cast<float>(kPauseLabelMinimumAlpha), 255.0F,
                  pause_flash_amount));

    // Adjust for any changes in the camera, especially regarding proportional
    // scaling.
    pause_label.position.x = frame.camera.Viewport().width * 0.5F;

    // 1.1^delta
    // Rolling harder on the mouse wheel will zoom in and out
    // faster compared to rolling the same distance slowly.
    const float zoom = std::clamp(
        frame.camera.zoom * std::pow(1.1F, frame.input.MouseWheelThisFrame().y),
        0.01F, 100.0F);
    frame.camera.zoom = zoom;

    const svanes::Rectangle2D view =
        frame.camera.ScreenToWorld(frame.camera.Viewport());
    svanes::Transform &background =
        frame.world.GetComponent<svanes::Transform>(background_entity);
    background.x = view.x;
    background.y = view.y;
    svanes::Rectangle2D &background_rectangle = std::get<svanes::Rectangle2D>(
        frame.world.GetComponent<svanes::SolidShape>(background_entity)
            .geometry);
    background_rectangle.width = view.width;
    background_rectangle.height = view.height;
}

bool OrbitalEscalationClient::ShouldQuit() const { return should_quit; }

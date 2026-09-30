#include "orbital_escalation_game.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

const svanes::TicCount kPauseFlashPeriod = svanes::SecondsToTics(1.0);
constexpr std::uint8_t kPauseLabelMinimumAlpha = 64;

void OrbitalEscalationGame::Initialize(svanes::GameContext &context) {
    // OrbitalSimulation advances gameplay itself. Letting the application
    // advance it too would move objects and their timelines twice.
    context.automatic_simulation = false;
    context.physics_step_tics = OrbitalStepTics;
    simulation = std::make_unique<OrbitalSimulation>(
        context.world, svanes::AssetPath("assets/orbitalEscalation"),
        context.concurrency);
    simulation->AddPlayer({1});
    const auto player = context.world.GetComponent<svanes::Transform>(
        *simulation->PlayerEntity({1}));
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
    context.camera.x = player.x;
    context.camera.y = player.y;
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

void OrbitalEscalationGame::Update(const svanes::FrameContext &frame) {
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    // Read input even on frames without a simulation step, so a brief click
    // between ticks is still available when the next tick consumes input.
    simulation->CaptureInput(frame, {1});

    // Keep at most one step of pending time. A slow frame slows the game down
    // instead of making it run several overdue steps at once.
    pending_tics +=
        std::min(frame.real_delta_tics, OrbitalStepTics - pending_tics);
    if (pending_tics == OrbitalStepTics) {
        const std::array inputs{simulation->TakeInput()};
        simulation->Step(inputs);
        pending_tics = 0;
    }

    // The pause label must keep animating while gameplay is stopped,
    // and so we keep its timeline separate from the overall game
    // timeline which we pause (otherwise pausing would pause it too)
    
    pause_timeline.Advance(frame.real_delta_tics);
    simulation->UpdateVisuals();
    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.camera.scale_mode =
            frame.camera.scale_mode == svanes::ScaleMode::Constant
                ? svanes::ScaleMode::Proportional
                : svanes::ScaleMode::Constant;
    }

    auto &pause_label =
        frame.world.GetComponent<svanes::TextLabel>(pause_label_entity);

    pause_label.visible = simulation->IsPaused();

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

    // Camera follows the player, centered on the screen.
    if (const auto entity = simulation->PlayerEntity({1})) {
        const svanes::Transform &player =
            frame.world.GetComponent<svanes::Transform>(*entity);
        frame.camera.x = player.x;
        frame.camera.y = player.y;
    }

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

bool OrbitalEscalationGame::ShouldQuit() const { return should_quit; }

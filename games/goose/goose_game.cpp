#include <svanes/asset_path.hpp>

/**
 * Where each of the six assigned tasks is implemented. Every entry is tagged inline
 * at the code it refers to, so searching this project for "TASK" finds all of them.
 *
 * - TASK 1, running the engine: main.cpp
 * - TASK 2A, static entity: the ground, in CreateArena in goose_simulation.cpp
 * - TASK 2B, controllable entity: the goose, spawned in Initialize below and built
 *   in Goose::Spawn in goose.cpp
 * - TASK 2C, auto-moving entity: the enemy, spawned in Initialize below and driven
 *   along its path in Update below
 * - TASK 3, physics: world gravity in Initialize below, the goose's Gravity and
 *   Kinematic2D components in Goose::Spawn, and flight in Goose::Update
 * - TASK 4, controls: input read into a GooseIntent in GooseSimulation::CaptureInput
 *   and TakeInput in goose_simulation.cpp, and applied in Goose::Update in goose.cpp
 * - TASK 5, collision response: Goose::ResolveCollisions in goose.cpp
 * - TASK 6, scaling: the Tab key in Update below
 */

#include "goose_game.hpp"

#include "bullets.hpp"
#include "peer_session.hpp"
#include "server_session.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <utility>
#include <vector>

namespace {

// how much larger than the view the sky is drawn, so its edges stay offscreen
constexpr float kSkyMargin = 1.5F;

// where in the window the goose is held, as a fraction of the output size, placing
// it centered horizontally and below center vertically to show more of what is ahead
constexpr float kCameraAnchorX = 0.5F;
constexpr float kCameraAnchorY = 0.68F;


}

GooseGame::GooseGame(GooseLaunch launch)
    : launch(std::move(launch))
{
}

void GooseGame::Initialize(svanes::GameContext& context)
{
    // the world gravity vector, applied by the engine every frame to any entity
    // holding both Kinematic2D and Gravity. Positive y is down
    context.gravity = {0.0F, svanes::PerSecondSquaredToPerTicSquared(GooseGravity)};

    background = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(background, svanes::Transform{
        .x = 960.0F,
        .y = 540.0F,
    });
    context.world.AddComponent<svanes::SolidShape>(background, svanes::SolidShape{
        .color = svanes::Color{.blue = 255},
        .geometry = svanes::Rectangle2D{
            .width = 1920.0F,
            .height = 1080.0F,
        },
    });
    // drawn behind everything else, and resized to cover the view each frame in Update
    context.world.AddComponent<svanes::ZOrder>(background, svanes::ZOrder{-100});

    const svanes::TextureHandle orb_texture =
        context.assets.LoadTexture(svanes::AssetPath(GOOSE_ASSETS_DIR) + "/darkworld_spawn_swirlingorb_idle.png");

    orb = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(orb, svanes::Transform{
        .x = 960.0F,
        .y = 540.0F,
    });
    context.world.AddComponent<svanes::Sprite>(orb, svanes::Sprite{
        .texture = orb_texture,
        .geometry = svanes::Rectangle2D{
            .width = 128.0F,
            .height = 128.0F,
        },
    });
    context.world.AddComponent<svanes::Timeline>(orb);
    context.world.AddComponent<svanes::SpriteAnimation>(orb, svanes::SpriteAnimation{
        .frame_width = 128,
        .frame_height = 128,
        .frame_count = 4,
        .tics_per_frame = svanes::SecondsToTics(0.12),
    });
    context.world.GetComponent<svanes::Sprite>(orb).source = svanes::Rectangle2D{
        64.0F, 64.0F, 128.0F, 128.0F};

    context.automatic_simulation = false;
    menu = std::make_unique<PauseMenu>(context, launch.server_host.has_value());
    simulation = std::make_unique<GooseSimulation>(context.world, context.gravity, Goose::LoadTextures(context.assets));
    if (launch.server_host) {
        session = std::make_unique<ServerSession>(*simulation, *launch.server_host);
    } else {
        session = std::make_unique<PeerSession>(*simulation, launch.port, launch.join_address);
    }
}

void GooseGame::Update(const svanes::FrameContext& frame)
{
    const bool alone = session->PlayerCount() == 1;
    if (!alone) {
        menu->ResetSpeed();
    }
    menu->HandleInput(frame, alone);
    if (menu->TakeQuitRequest()) {
        session->RequestLeave();
    }

    // TASK 6, scaling: Tab switches the camera between the two scale modes.
    // Constant draws everything at its literal pixel size, so resizing the window
    // reveals more of the world. Proportional rescales with the window, so
    // everything keeps the same fraction of the screen at any size.
    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.camera.scale_mode = frame.camera.scale_mode == svanes::ScaleMode::Constant
            ? svanes::ScaleMode::Proportional
            : svanes::ScaleMode::Constant;
    }

    simulation->SetControlsEnabled(!menu->IsOpen());
    session->Update(frame, ClockRequest{.speed = menu->Speed(), .paused = menu->IsOpen()});
    should_quit = should_quit || session->CanClose();
    ReportStatus(session->Status());
    if (session->IsRunning()) {
        UpdateCamera(frame, simulation->PlayerEntity(session->LocalPeer()));
    }
    menu->Draw(frame, alone);
}

void GooseGame::ReportStatus(const std::string& status)
{
    if (status != last_network_status) {
        std::cout << status << '\n';
        last_network_status = status;
    }
}

void GooseGame::UpdateCamera(const svanes::FrameContext& frame, svanes::Entity player)
{

    // the camera follows by finding which world position currently sits at the
    // anchor point on screen, then shifting by however far the goose is from it.
    // Working through the anchor this way keeps the follow correct at any zoom,
    // scale mode or window size, since the camera itself does that conversion.
    const svanes::Transform& goose_transform = frame.world.GetComponent<svanes::Transform>(player);
    const svanes::Rectangle2D anchor = frame.camera.ScreenToWorld(
        {frame.output_width * kCameraAnchorX, frame.output_height * kCameraAnchorY, 0.0F, 0.0F}
    );

    frame.camera.x += goose_transform.x - anchor.x;
    frame.camera.y += goose_transform.y - anchor.y;

    const svanes::Rectangle2D view = frame.camera.ScreenToWorld({
        frame.output_width * 0.5F, frame.output_height * 0.5F,
        static_cast<float>(frame.output_width), static_cast<float>(frame.output_height)
    });

    svanes::Transform& sky = frame.world.GetComponent<svanes::Transform>(background);
    sky.x = view.x;
    sky.y = view.y;

    svanes::Rectangle2D& sky_body = std::get<svanes::Rectangle2D>(
        frame.world.GetComponent<svanes::SolidShape>(background).geometry
    );
    sky_body.width = view.width * kSkyMargin;
    sky_body.height = view.height * kSkyMargin;

}

svanes::TicCount GooseGame::GetFrameIntervalTics() const
{
    if (!launch.server_host) {
        return 0;
    }
    switch (menu->LoopRate()) {
    case GooseSpeed::Half:
        return svanes::SecondsToTics(1.0 / 30.0);
    case GooseSpeed::Normal:
        return svanes::SecondsToTics(1.0 / 60.0);
    case GooseSpeed::Double:
        return svanes::SecondsToTics(1.0 / 120.0);
    }
    throw std::invalid_argument("GooseGame received an unknown loop rate.");
}

bool GooseGame::ShouldQuit() const
{
    return should_quit;
}

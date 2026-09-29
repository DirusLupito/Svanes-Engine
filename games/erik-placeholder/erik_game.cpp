#include <svanes/asset_path.hpp>

/**
 * Where each of the six assigned tasks is implemented. Every entry is tagged inline
 * at the code it refers to, so searching this project for "TASK" finds all of them.
 *
 * - TASK 1, running the engine: main.cpp
 * - TASK 2A, static entity: the ground, in Initialize below
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

#include "erik_game.hpp"

#include "bullets.hpp"

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
#include <string>
#include <variant>
#include <utility>
#include <vector>

namespace {

// the playable area, enclosed by the ground below and border walls on the other
// three sides
constexpr float kWorldLeft = 0.0F;
constexpr float kWorldRight = 6000.0F;
constexpr float kWorldTop = -1200.0F;
constexpr float kGroundTop = 974.0F;
constexpr float kGroundBottom = 1080.0F;
constexpr float kBorderThickness = 100.0F;

// how much larger than the view the sky is drawn, so its edges stay offscreen
constexpr float kSkyMargin = 1.5F;

// where in the window the goose is held, as a fraction of the output size, placing
// it centered horizontally and below center vertically to show more of what is ahead
constexpr float kCameraAnchorX = 0.5F;
constexpr float kCameraAnchorY = 0.68F;

/**
 * Creates one piece of immovable world geometry, drawn as a colored rectangle and
 * tagged Solid so the goose collides with it.
 *
 * @param world The registry the entity is created in.
 * @param center_x The world x position of the block's center.
 * @param center_y The world y position of the block's center.
 * @param width The block's width.
 * @param height The block's height.
 * @param color The color the block is drawn in.
 */
void CreateSolidBlock(
    svanes::Registry& world, float center_x, float center_y,
    float width, float height, svanes::Color color
)
{
    const svanes::Rectangle2D body{
        .width = width,
        .height = height,
    };

    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{
        .x = center_x,
        .y = center_y,
    });
    world.AddComponent<svanes::SolidShape>(entity, svanes::SolidShape{
        .color = color,
        .geometry = body,
    });
    world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
    world.AddComponent<Solid>(entity);
}

/**
 * @param sync_settings The input sync pacing and limits.
 * @return A hash of everything two builds must share to play together.
 */
std::uint64_t RulesHash(const svanes::SyncSettings& sync_settings)
{
    svanes::MessageWriter writer;
    writer.WriteUint64(sync_settings.step_tics);
    writer.WriteFloat32(GooseGravity);
    writer.WriteUint64(sync_settings.prediction_ticks);
    writer.WriteUint64(sync_settings.history_ticks);
    writer.WriteUint64(sync_settings.hash_interval_ticks);
    writer.WriteUint64(sync_settings.max_unchecked_ticks);
    return svanes::HashBytes(writer.Finish().bytes);
}

}

ErikGame::ErikGame(std::uint16_t port, std::optional<svanes::UdpAddress> join_address)
    : port(port), join_address(std::move(join_address))
{
}

void ErikGame::Initialize(svanes::GameContext& context)
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

    const float world_width = kWorldRight - kWorldLeft;
    const float world_center_x = (kWorldLeft + kWorldRight) * 0.5F;
    const float border_center_y = (kWorldTop + kGroundBottom) * 0.5F;
    const float border_height = kGroundBottom - kWorldTop;

    // TASK 2A, static entity: the ground. A Transform, a SolidShape to draw, a
    // Collider2D to be hit, and the Solid tag, with no motion components at all, so
    // nothing the engine does each frame can move it.
    CreateSolidBlock(
        context.world, world_center_x, (kGroundTop + kGroundBottom) * 0.5F,
        world_width, kGroundBottom - kGroundTop, svanes::Color{.red = 60, .green = 140, .blue = 70}
    );

    // the left, right and top walls, built the same way, closing off the arena
    CreateSolidBlock(
        context.world, kWorldLeft - kBorderThickness * 0.5F, border_center_y,
        kBorderThickness, border_height, svanes::Color{.red = 90, .green = 90, .blue = 110}
    );

    CreateSolidBlock(
        context.world, kWorldRight + kBorderThickness * 0.5F, border_center_y,
        kBorderThickness, border_height, svanes::Color{.red = 90, .green = 90, .blue = 110}
    );

    CreateSolidBlock(
        context.world, world_center_x, kWorldTop - kBorderThickness * 0.5F,
        world_width + 2.0F * kBorderThickness, kBorderThickness,
        svanes::Color{.red = 90, .green = 90, .blue = 110}
    );

    const svanes::TextureHandle orb_texture =
        context.assets.LoadTexture(svanes::AssetPath(ERIK_GAME_ASSETS_DIR) + "/darkworld_spawn_swirlingorb_idle.png");

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

    simulation = std::make_unique<GooseSimulation>(context);
    const svanes::SyncSettings sync_settings{.step_tics = GooseStepTics};
    const svanes::PeerSettings settings{GooseSession, GooseMaxPlayers, RulesHash(sync_settings)};
    auto pipe = std::make_unique<svanes::UdpMsgPipe>(port);
    if (join_address) {
        network = std::make_unique<svanes::PeerGroup>(std::move(pipe), *join_address, settings);
    } else {
        network = std::make_unique<svanes::PeerGroup>(std::move(pipe), settings);
    }
    sync = std::make_unique<svanes::InputSync<GooseIntent, GooseIntentUse>>(*network, *simulation, sync_settings);
    if (join_address) {
        return;
    }
    std::cout << "Started a new world. Others can join on port " << network->Port() << ".\n";
}

void ErikGame::Update(const svanes::FrameContext& frame)
{
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        sync->RequestLeave();
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

    const bool was_running = network->IsRunning();
    sync->Update(frame);
    should_quit = should_quit || sync->CanClose();
    if (!was_running && network->IsRunning()) {
        std::cout << "Joined at tick " << sync->Tick() << " with "
            << network->Peers().size() << " player(s). Others can join on port " << network->Port() << ".\n";
    }
    if (!network->IsRunning()) {
        ReportStatus(sync->Status());
        return;
    }
    ReportStatus("Peer " + std::to_string(network->LocalPeer().value) + ": " + sync->Status());
    network_diagnostic_tics += std::min(frame.real_delta_tics,
        svanes::TicsPerSecond - network_diagnostic_tics);
    if (network_diagnostic_tics >= svanes::TicsPerSecond) {
        std::cout << "Peer " << network->LocalPeer().value << ": "
            << sync->Diagnostics() << '\n';
        network_diagnostic_tics = 0;
    }
    UpdateCamera(frame, simulation->PlayerEntity(network->LocalPeer()));
}

void ErikGame::ReportStatus(const std::string& status)
{
    if (status != last_network_status) {
        std::cout << status << '\n';
        last_network_status = status;
    }
}

void ErikGame::UpdateCamera(const svanes::FrameContext& frame, svanes::Entity player)
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

bool ErikGame::ShouldQuit() const
{
    return should_quit;
}

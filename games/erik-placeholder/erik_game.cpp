#include "erik_game.hpp"

#include "bullets.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <string>

namespace {

constexpr svanes::Rectangle2D kBulletBounds{960.0F, 540.0F, 2720.0F, 1880.0F};

}

void ErikGame::Initialize(svanes::GameContext& context)
{
    context.gravity = {0.0F, 2000.0F};

    const svanes::Entity background = context.world.CreateEntity();
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
    context.world.AddComponent<svanes::ZOrder>(background, svanes::ZOrder{-100});

    const svanes::Rectangle2D ground_body{
        .width = 1920.0F,
        .height = 106.0F,
    };

    const svanes::Entity ground = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(ground, svanes::Transform{
        .x = 960.0F,
        .y = 1027.0F,
    });
    context.world.AddComponent<svanes::SolidShape>(ground, svanes::SolidShape{
        .color = svanes::Color{.red = 60, .green = 140, .blue = 70},
        .geometry = ground_body,
    });
    context.world.AddComponent<svanes::Collider2D>(ground, svanes::Collider2D{ground_body});
    context.world.AddComponent<Solid>(ground);

    const svanes::TextureHandle orb_texture =
        context.assets.LoadTexture(std::string{ERIK_GAME_ASSETS_DIR} + "/darkworld_spawn_swirlingorb_idle.png");

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
    context.world.AddComponent<svanes::SpriteAnimation>(orb, svanes::SpriteAnimation{
        .frame_width = 128,
        .frame_height = 128,
        .frame_count = 4,
        .seconds_per_frame = 0.12F,
    });

    goose.Spawn(context, 960.0F, 700.0F);
}

void ErikGame::Update(const svanes::FrameContext& frame)
{
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        should_quit = true;
    }

    if (frame.input.WasPressed(svanes::Key::Tab)) {
        frame.scale_mode = frame.scale_mode == svanes::ScaleMode::Constant
            ? svanes::ScaleMode::Proportional
            : svanes::ScaleMode::Constant;
    }

    GooseIntent intent{};
    if (frame.input.IsDown(svanes::Key::D)) {
        intent.move.x += 1.0F;
    }
    if (frame.input.IsDown(svanes::Key::A)) {
        intent.move.x -= 1.0F;
    }
    intent.jump = frame.input.WasPressed(svanes::Key::Space);
    intent.fire = frame.input.IsMouseButtonDown(svanes::MouseButton::Left);
    intent.aim_point = svanes::ScreenToWorldPoint(
        frame.camera, frame.input.MousePosition(),
        frame.output_width, frame.output_height, frame.scale_mode
    );

    goose.Update(frame, intent);

    UpdateBullets(frame.world, kBulletBounds);
}

bool ErikGame::ShouldQuit() const
{
    return should_quit;
}

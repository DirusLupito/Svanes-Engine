#include "erik_game.hpp"

#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <string>

class Goose {
    

    const svanes::TextureHandle goose_walk_texture =
        context.assets.LoadTexture(std::string{ERIK_GAME_ASSETS_DIR} + "/goose.png");
    svanes::Entity goose = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(goose, svanes::Transform{
        .x = 960.0F,
        .y = 700.0F,
    });
    context.world.AddComponent<svanes::Sprite>(goose, svanes::Sprite{
        .texture = goose_walk_texture,
        .geometry = svanes::Rectangle2D{
            .width = 29.F,
            .height = 27.F,
        },
    });
    // context.worldAddComponent<svanes::SpriteAnimation

}

void ErikGame::Initialize(svanes::GameContext& context)
{
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
}

bool ErikGame::ShouldQuit() const
{
    return should_quit;
}

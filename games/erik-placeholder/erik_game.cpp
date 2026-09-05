#include "erik_game.hpp"

#include <svanes/input.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <string>

void ErikGame::Initialize(svanes::GameContext& context)
{
    const svanes::TextureHandle orb_texture =
        context.assets.LoadTexture(std::string{ERIK_GAME_ASSETS_DIR} + "/darkworld_spawn_swirlingorb_idle.png");

    orb = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(orb, svanes::Transform{
        .width = 128.0F,
        .height = 128.0F,
    });
    context.world.AddComponent<svanes::Sprite>(orb, svanes::Sprite{
        .texture = orb_texture,
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

    svanes::Transform& transform = frame.world.GetComponent<svanes::Transform>(orb);
    transform.x = (static_cast<float>(frame.output_width) - transform.width) * 0.5F;
    transform.y = (static_cast<float>(frame.output_height) - transform.height) * 0.5F;
}

bool ErikGame::ShouldQuit() const
{
    return should_quit;
}

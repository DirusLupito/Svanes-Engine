#include "erik_game.hpp"

#include <svanes/input.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <SDL3_image/SDL_image.h>

#include <string>

ErikGame::~ErikGame()
{
    if (orb_texture != nullptr) {
        SDL_DestroyTexture(orb_texture);
    }
}

void ErikGame::EnsureOrbLoaded(SDL_Renderer* renderer)
{
    if (orb_texture != nullptr) {
        return;
    }

    const char* base_path = SDL_GetBasePath();
    const std::string texture_path = (base_path != nullptr ? base_path : "") + std::string{"darkworld_spawn_swirlingorb_idle.png"};

    orb_texture = IMG_LoadTexture(renderer, texture_path.c_str());
    if (orb_texture == nullptr) {
        SDL_Log("Could not load orb texture: %s", SDL_GetError());
        return;
    }

    SDL_SetTextureScaleMode(orb_texture, SDL_SCALEMODE_NEAREST);

    orb = registry.CreateEntity();
    registry.AddComponent<svanes::Transform>(orb);
    registry.AddComponent<svanes::SpriteAnimation>(orb, svanes::SpriteAnimation{
        .texture = orb_texture,
        .frame_width = 128,
        .frame_height = 128,
        .frame_count = 4,
        .seconds_per_frame = 0.12F,
    });
}

void ErikGame::OnUpdate(float delta_seconds, const svanes::InputManager& input)
{
    if (input.WasPressed(SDL_SCANCODE_ESCAPE)) {
        should_quit = true;
    }

    svanes::AdvanceSpriteAnimations(registry, delta_seconds);
}

bool ErikGame::ShouldQuit() const
{
    return should_quit;
}

void ErikGame::OnRender(SDL_Renderer* renderer, int32_t output_width, int32_t output_height)
{
    EnsureOrbLoaded(renderer);

    SDL_SetRenderDrawColor(renderer, 17, 24, 39, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    if (registry.HasComponent<svanes::Transform>(orb)) {
        svanes::Transform& transform = registry.GetComponent<svanes::Transform>(orb);
        const svanes::SpriteAnimation& animation = registry.GetComponent<svanes::SpriteAnimation>(orb);
        transform.x = (static_cast<float>(output_width) - static_cast<float>(animation.frame_width)) * 0.5F;
        transform.y = (static_cast<float>(output_height) - static_cast<float>(animation.frame_height)) * 0.5F;
    }

    svanes::RenderSprites(registry, renderer);
}

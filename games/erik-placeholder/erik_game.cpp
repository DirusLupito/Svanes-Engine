#include "erik_game.hpp"

#include <svanes/components/sprite_animation.hpp>
#include <svanes/components/transform.hpp>
#include <svanes/input.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <SDL3_image/SDL_image.h>

#include <string>

ErikGame::~ErikGame()
{
    if (orb_texture_ != nullptr) {
        SDL_DestroyTexture(orb_texture_);
    }
}

void ErikGame::EnsureOrbLoaded(SDL_Renderer* renderer)
{
    if (orb_texture_ != nullptr) {
        return;
    }

    const char* base_path = SDL_GetBasePath();
    const std::string texture_path = (base_path != nullptr ? base_path : "") + std::string{"darkworld_spawn_swirlingorb_idle.png"};

    orb_texture_ = IMG_LoadTexture(renderer, texture_path.c_str());
    if (orb_texture_ == nullptr) {
        SDL_Log("Could not load orb texture: %s", SDL_GetError());
        return;
    }

    SDL_SetTextureScaleMode(orb_texture_, SDL_SCALEMODE_NEAREST);

    orb_ = registry_.CreateEntity();
    registry_.AddComponent<svanes::Transform>(orb_);
    registry_.AddComponent<svanes::SpriteAnimation>(orb_, svanes::SpriteAnimation{
        .texture = orb_texture_,
        .frame_width = 128,
        .frame_height = 128,
        .frame_count = 4,
        .seconds_per_frame = 0.12F,
    });
}

void ErikGame::OnUpdate(float delta_seconds, const svanes::InputManager& input)
{
    if (input.WasPressed(SDL_SCANCODE_ESCAPE)) {
        should_quit_ = true;
    }

    svanes::AdvanceSpriteAnimations(registry_, delta_seconds);
}

bool ErikGame::ShouldQuit() const
{
    return should_quit_;
}

void ErikGame::OnRender(SDL_Renderer* renderer, int output_width, int output_height)
{
    EnsureOrbLoaded(renderer);

    SDL_SetRenderDrawColor(renderer, 17, 24, 39, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    if (svanes::Transform* transform = registry_.TryGetComponent<svanes::Transform>(orb_)) {
        const svanes::SpriteAnimation& animation = registry_.GetComponent<svanes::SpriteAnimation>(orb_);
        transform->x = (static_cast<float>(output_width) - static_cast<float>(animation.frame_width)) * 0.5F;
        transform->y = (static_cast<float>(output_height) - static_cast<float>(animation.frame_height)) * 0.5F;
    }

    svanes::RenderSprites(registry_, renderer);
}

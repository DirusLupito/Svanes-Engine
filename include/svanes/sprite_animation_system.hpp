#pragma once

#include <svanes/render/render_system.hpp>

#include <SDL3/SDL.h>

namespace svanes {

class Registry;

/**
 * Represents a sprite animation in a 2D game.
 *
 * texture - Pointer to the SDL_Texture containing the sprite sheet.
 * frame_width - Width of each frame in the sprite sheet.
 * frame_height - Height of each frame in the sprite sheet.
 * frame_count - Total number of frames in the animation.
 * current_frame - Index of the current frame being displayed.
 * seconds_per_frame - Duration in seconds for each frame.
 * elapsed_seconds - Accumulated time since the last frame change. Used in `AdvanceSpriteAnimations()` to determine when to advance the animation (and by how far, if applicable).
 */
struct SpriteAnimation {
    SDL_Texture* texture = nullptr;
    int32_t frame_width = 0;
    int32_t frame_height = 0;
    int32_t frame_count = 1;
    int32_t current_frame = 0;
    float seconds_per_frame = 0.1F;
    float elapsed_seconds = 0.0F;
};

void AdvanceSpriteAnimations(Registry& registry, float delta_seconds);
void RenderSprites(const Registry& registry, SDL_Renderer* renderer);

}

#pragma once

#include <SDL3/SDL.h>

namespace svanes {

struct SpriteAnimation {
    SDL_Texture* texture = nullptr;
    int frame_width = 0;
    int frame_height = 0;
    int frame_count = 1;
    int current_frame = 0;
    float seconds_per_frame = 0.1F;
    float elapsed_seconds = 0.0F;
};

} // namespace svanes

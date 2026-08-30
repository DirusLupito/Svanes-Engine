#pragma once

#include <SDL3/SDL.h>

namespace svanes {

class Registry;

void AdvanceSpriteAnimations(Registry& registry, float delta_seconds);
void RenderSprites(Registry& registry, SDL_Renderer* renderer);

} // namespace svanes

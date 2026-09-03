#pragma once

#include <SDL3/SDL.h>

namespace svanes {

class InputManager;

class IGame {
public:
    virtual ~IGame() = default;

    virtual void OnUpdate(float delta_seconds, const InputManager& input) = 0;
    virtual void OnRender(SDL_Renderer* renderer, int32_t output_width, int32_t output_height) = 0;

    virtual bool ShouldQuit() const { return false; }
};

} // namespace svanes

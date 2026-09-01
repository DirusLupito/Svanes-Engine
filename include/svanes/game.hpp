#pragma once

#include <SDL3/SDL.h>

namespace svanes {

class InputManager;

class IGame {
public:
    virtual ~IGame() = default;

    virtual void OnUpdate(float delta_seconds, const InputManager& input) = 0;
    virtual void OnRender(SDL_Renderer* renderer, int output_width, int output_height) = 0;

    virtual bool ShouldQuit() const { return false; }
};

} // namespace svanes

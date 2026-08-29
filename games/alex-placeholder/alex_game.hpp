#pragma once

#include <svanes/game.hpp>

class AlexGame final : public svanes::IGame {
public:
    void OnUpdate(float delta_seconds, const svanes::InputManager& input) override;
    void OnRender(SDL_Renderer* renderer, int output_width, int output_height) override;

private:
    float elapsed_seconds_ = 0.0F;
};

#pragma once

#include <svanes/game.hpp>

class AbmGame final : public svanes::IGame {
public:
    ~AbmGame() override;

    void OnUpdate(float delta_seconds, const svanes::InputManager& input) override;
    void OnRender(SDL_Renderer* renderer, int32_t output_width, int32_t output_height) override;

private:
    float elapsed_seconds = 0.0F;
    SDL_Texture* gradient_texture = nullptr;
};

#pragma once

#include <svanes/game.hpp>

class SandboxGame final : public svanes::IGame {
public:
    void Initialize(svanes::GameContext& context) override;
    void Update(const svanes::FrameContext& frame) override;
    void BuildRenderQueue(svanes::RenderQueue& render_queue) override;
    void OnRender(SDL_Renderer* renderer, int32_t output_width, int32_t output_height) override;

private:
    float elapsed_seconds = 0.0F;
};

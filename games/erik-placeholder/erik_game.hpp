#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/registry.hpp>

class ErikGame final : public svanes::IGame {
public:
    ~ErikGame() override;

    void OnUpdate(float delta_seconds, const svanes::InputManager& input) override;
    void OnRender(SDL_Renderer* renderer, int output_width, int output_height) override;
    [[nodiscard]] bool ShouldQuit() const override;

private:
    void EnsureOrbLoaded(SDL_Renderer* renderer);

    svanes::Registry registry_;
    svanes::Entity orb_{};
    SDL_Texture* orb_texture_ = nullptr;
    bool should_quit_ = false;
};

#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>

#include "goose.hpp"

class ErikGame final : public svanes::IGame {
public:
    void Initialize(svanes::GameContext& context) override;
    void Update(const svanes::FrameContext& frame) override;
    bool ShouldQuit() const override;

private:
    svanes::Entity orb{};
    svanes::Entity background{};
    svanes::Entity enemy{};
    Goose goose;
    float elapsed_seconds = 0.0F;
    float enemy_fire_cooldown = 0.0F;
    float left_tap_timer = 0.0F;
    float right_tap_timer = 0.0F;
    bool should_quit = false;
};

#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>

class ErikGame final : public svanes::IGame {
public:
    void Initialize(svanes::GameContext& context) override;
    void Update(const svanes::FrameContext& frame) override;
    bool ShouldQuit() const override;

private:
    svanes::Entity orb{};
    bool should_quit = false;
};

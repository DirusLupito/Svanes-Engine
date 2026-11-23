#pragma once

#include <svanes/game.hpp>

namespace nutritionally_accurate_snake {

class Snake final : public svanes::IGame {
public:
    void Initialize(svanes::GameContext &context) override;
    void Update(const svanes::FrameContext &frame) override;
    bool ShouldQuit() const override;
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    bool quit = false;
};

}

#pragma once

#include <svanes/game.hpp>
#include <svanes/memory/object_pool.hpp>

namespace nutritionally_accurate_snake {

class SnakeHead final {
public:
    explicit SnakeHead(svanes::Registry &world);

private:
    svanes::Entity entity;
};

class Snake final : public svanes::IGame {
public:
    void Initialize(svanes::GameContext &context) override;
    void Update(const svanes::FrameContext &frame) override;
    bool ShouldQuit() const override;
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    svanes::ObjectPool<SnakeHead> head_pool{1};
    svanes::ObjectPool<SnakeHead>::Pointer head;
    bool quit = false;
};

}

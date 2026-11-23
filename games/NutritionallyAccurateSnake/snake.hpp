#pragma once

#include <svanes/game.hpp>
#include <svanes/memory/object_pool.hpp>

#include <deque>
#include <vector>

namespace nutritionally_accurate_snake {

class SnakeHead final {
public:
    explicit SnakeHead(svanes::Registry &world);
    void PhysicsUpdate(const svanes::PhysicsContext &physics);
    svanes::Entity GetEntity() const;

private:
    svanes::Entity entity;
};

class SnakeBody final {
public:
    SnakeBody(svanes::Registry &world, svanes::Vector2D position);
    void SetPosition(svanes::Registry &world, svanes::Vector2D position);
    void SetRadius(svanes::Registry &world, float radius);

private:
    svanes::Entity entity;
};

class Snake final : public svanes::IGame {
public:
    void Initialize(svanes::GameContext &context) override;
    void PhysicsUpdate(const svanes::PhysicsContext &physics) override;
    void Update(const svanes::FrameContext &frame) override;
    bool ShouldQuit() const override;
    svanes::TicCount GetFrameIntervalTics() const override;

private:
    void UpdateBody(svanes::Registry &world);

    svanes::ObjectPool<SnakeHead> head_pool{1};
    svanes::ObjectPool<SnakeHead>::Pointer head;
    svanes::ObjectPool<SnakeBody> body_pool{3};
    std::vector<svanes::ObjectPool<SnakeBody>::Pointer> body;
    std::deque<svanes::Vector2D> path;
    bool quit = false;
};

}

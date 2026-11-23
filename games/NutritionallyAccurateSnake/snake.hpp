#pragma once

#include <svanes/game.hpp>
#include <svanes/memory/object_pool.hpp>

#include <array>
#include <deque>
#include <vector>

namespace svanes {
class CollisionEvent;
}

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
    svanes::Entity GetEntity() const;

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
    void InitializeEdges(svanes::GameContext &context);
    void HandleCollision(svanes::Registry &world,
                         const svanes::CollisionEvent &event);

    svanes::ObjectPool<SnakeHead> head_pool{1};
    svanes::ObjectPool<SnakeHead>::Pointer head;
    svanes::ObjectPool<SnakeBody> body_pool{3};
    std::vector<svanes::ObjectPool<SnakeBody>::Pointer> body;
    std::deque<svanes::Vector2D> path;
    std::array<svanes::Entity, 4> edges{};
    svanes::Entity collision_label{};
    svanes::EventManager *events = nullptr;
    bool quit = false;
};

}

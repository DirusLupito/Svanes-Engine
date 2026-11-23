#pragma once

#include <svanes/game.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/memory/object_pool.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <random>
#include <vector>

namespace svanes {
class CollisionEvent;
}

namespace nutritionally_accurate_snake {

enum class PelletType : std::uint8_t {
    Orange,
    Red,
    LightBlue,
    DarkBlue,
};

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
    void ResetSnake(svanes::Registry &world);
    void UpdateBody(svanes::Registry &world);
    void InitializeEdges(svanes::GameContext &context);
    void SpawnFood(svanes::Registry &world);
    void UpdateGameOverScreen(const svanes::FrameContext &frame);
    void HandleCollision(svanes::Registry &world,
                         const svanes::CollisionEvent &event);

    svanes::ObjectPool<SnakeHead> head_pool{1};
    svanes::ObjectPool<SnakeHead>::Pointer head;
    svanes::ObjectPool<SnakeBody> body_pool{3};
    std::vector<svanes::ObjectPool<SnakeBody>::Pointer> body;
    std::deque<svanes::Vector2D> path;
    std::vector<svanes::Entity> pellets;
    svanes::Rectangle2D food_spawn_area{};
    svanes::TicCount food_spawn_tics = 0;
    std::uint64_t score = 0;
    std::mt19937 random{std::random_device{}()};
    std::array<svanes::Entity, 4> edges{};
    std::array<svanes::Entity, 3> game_over_shapes{};
    std::array<svanes::Entity, 3> game_over_labels{};
    svanes::EventManager *events = nullptr;
    bool game_over = false;
    bool quit = false;
};

}

#include "snake.hpp"

#include <svanes/deterministic_math.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace nutritionally_accurate_snake {

constexpr float kMoveSpeed = 180.0F;
constexpr float kTurnRate = std::numbers::pi_v<float>;
constexpr float kRadius = 16.0F;
constexpr float kBodySpacing = 16.0F;
constexpr std::size_t kInitialBodyCount = 3;

SnakeHead::SnakeHead(svanes::Registry &world) : entity(world.CreateEntity()) {
    constexpr float heading = std::numbers::pi_v<float> / 6.0F;
    const float speed = svanes::PerSecondToPerTic(kMoveSpeed);

    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.rotation = heading});
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{.color = {80, 200, 100, 255},
                           .geometry = svanes::Circle2D{.radius = kRadius}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{1});
    world.AddComponent<svanes::Timeline>(entity);
    world.AddComponent<svanes::Kinematic2D>(
        entity,
        svanes::Kinematic2D{.velocity_x = svanes::Cos(heading) * speed,
                            .velocity_y = svanes::Sin(heading) * speed});
}

void SnakeHead::PhysicsUpdate(const svanes::PhysicsContext &physics) {
    const float direction_x =
        static_cast<float>(physics.input.IsDown(svanes::Key::Right) -
                           physics.input.IsDown(svanes::Key::Left));
    const float direction_y =
        static_cast<float>(physics.input.IsDown(svanes::Key::Down) -
                           physics.input.IsDown(svanes::Key::Up));
    if (direction_x == 0.0F && direction_y == 0.0F) {
        return;
    }

    const auto delta_tics =
        physics.world.GetComponent<svanes::Timeline>(entity).GetDeltaTics();
    auto &transform = physics.world.GetComponent<svanes::Transform>(entity);
    const float target = std::atan2(direction_y, direction_x);
    const float difference = std::remainder(target - transform.rotation,
                                            2.0F * std::numbers::pi_v<float>);
    const float max_turn =
        svanes::PerSecondToPerTic(kTurnRate) * static_cast<float>(delta_tics);
    transform.rotation = std::remainder(
        transform.rotation + std::clamp(difference, -max_turn, max_turn),
        2.0F * std::numbers::pi_v<float>);

    const float speed = svanes::PerSecondToPerTic(kMoveSpeed);
    auto &motion = physics.world.GetComponent<svanes::Kinematic2D>(entity);
    motion.velocity_x = svanes::Cos(transform.rotation) * speed;
    motion.velocity_y = svanes::Sin(transform.rotation) * speed;
}

svanes::Entity SnakeHead::GetEntity() const { return entity; }

SnakeBody::SnakeBody(svanes::Registry &world, svanes::Vector2D position)
    : entity(world.CreateEntity()) {
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{.color = {60, 160, 80, 255},
                           .geometry = svanes::Circle2D{.radius = kRadius}});
}

void SnakeBody::SetPosition(svanes::Registry &world,
                            svanes::Vector2D position) {
    auto &transform = world.GetComponent<svanes::Transform>(entity);
    transform.x = position.x;
    transform.y = position.y;
}

void SnakeBody::SetRadius(svanes::Registry &world, float radius) {
    if (!std::isfinite(radius) || radius <= 0.0F) {
        throw std::invalid_argument(
            "Snake body radius must be finite and positive.");
    }
    auto &shape = world.GetComponent<svanes::SolidShape>(entity);
    std::get<svanes::Circle2D>(shape.geometry).radius = radius;
}

void Snake::Initialize(svanes::GameContext &context) {
    head = head_pool.Make(context.world);
    const auto &transform =
        context.world.GetComponent<svanes::Transform>(head->GetEntity());
    const svanes::Vector2D position{transform.x, transform.y};
    const svanes::Vector2D forward{svanes::Cos(transform.rotation),
                                   svanes::Sin(transform.rotation)};
    path.push_back(position);
    path.push_back(
        position -
        forward * (kBodySpacing * static_cast<float>(kInitialBodyCount + 1)));
    body.reserve(kInitialBodyCount);
    for (std::size_t index = 0; index < kInitialBodyCount; ++index) {
        body.push_back(body_pool.Make(
            context.world,
            position -
                forward * (kBodySpacing * static_cast<float>(index + 1))));
    }
}

void Snake::PhysicsUpdate(const svanes::PhysicsContext &physics) {
    head->PhysicsUpdate(physics);
    UpdateBody(physics.world);
}

void Snake::UpdateBody(svanes::Registry &world) {
    const auto &transform =
        world.GetComponent<svanes::Transform>(head->GetEntity());
    const svanes::Vector2D position{transform.x, transform.y};
    if (position.x != path.front().x || position.y != path.front().y) {
        path.push_front(position);
    }

    std::size_t point = 0;
    float distance = 0.0F;
    for (std::size_t index = 0; index < body.size(); ++index) {
        const float target = kBodySpacing * static_cast<float>(index + 1);
        while (point + 1 < path.size()) {
            const auto direction = path[point + 1] - path[point];
            const float length = svanes::Length(direction.x, direction.y);
            if (distance + length >= target) {
                body[index]->SetPosition(
                    world,
                    path[point] + direction * ((target - distance) / length));
                break;
            }
            distance += length;
            ++point;
        }
        if (point + 1 == path.size()) {
            throw std::logic_error(
                "Snake body extends beyond its recorded path.");
        }
    }
    while (path.size() > point + 2) {
        path.pop_back();
    }
}

void Snake::Update(const svanes::FrameContext &frame) {
    quit = frame.input.WasPressed(svanes::Key::Escape);
}

bool Snake::ShouldQuit() const { return quit; }

svanes::TicCount Snake::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

}

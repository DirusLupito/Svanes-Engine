#include "snake.hpp"

#include <svanes/deterministic_math.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace nutritionally_accurate_snake {

constexpr float kMoveSpeed = 180.0F;
constexpr float kTurnRate = std::numbers::pi_v<float>;

SnakeHead::SnakeHead(svanes::Registry &world) : entity(world.CreateEntity()) {
    constexpr float heading = std::numbers::pi_v<float> / 6.0F;
    const float speed = svanes::PerSecondToPerTic(kMoveSpeed);

    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.rotation = heading});
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{.color = {80, 200, 100, 255},
                           .geometry = svanes::Circle2D{.radius = 16.0F}});
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

void Snake::Initialize(svanes::GameContext &context) {
    head = head_pool.Make(context.world);
}

void Snake::PhysicsUpdate(const svanes::PhysicsContext &physics) {
    head->PhysicsUpdate(physics);
}

void Snake::Update(const svanes::FrameContext &frame) {
    quit = frame.input.WasPressed(svanes::Key::Escape);
}

bool Snake::ShouldQuit() const { return quit; }

svanes::TicCount Snake::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

}

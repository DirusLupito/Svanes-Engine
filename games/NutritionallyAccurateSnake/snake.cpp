#include "snake.hpp"

#include <svanes/deterministic_math.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <SDL3/SDL.h>

#include <numbers>

namespace nutritionally_accurate_snake {

SnakeHead::SnakeHead(svanes::Registry &world) : entity(world.CreateEntity()) {
    constexpr float heading = std::numbers::pi_v<float> / 6.0F;
    const float speed = svanes::PerSecondToPerTic(180.0F);

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

void Snake::Initialize(svanes::GameContext &context) {
    head = head_pool.Make(context.world);
}

void Snake::Update(const svanes::FrameContext &frame) {
    quit = frame.input.WasPressed(svanes::Key::Escape) ||
           SDL_HasEvent(SDL_EVENT_QUIT) ||
           SDL_HasEvent(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
}

bool Snake::ShouldQuit() const { return quit; }

svanes::TicCount Snake::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

}

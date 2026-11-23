#include "snake.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/collision_pass.hpp>
#include <svanes/deterministic_math.hpp>
#include <svanes/events/collision_event.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/input/input_manager.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>

namespace nutritionally_accurate_snake {

constexpr float kMoveSpeed = 180.0F;
constexpr float kTurnRate = std::numbers::pi_v<float>;
constexpr float kRadius = 16.0F;
constexpr float kBodySpacing = 16.0F;
constexpr std::size_t kInitialBodyCount = 3;
constexpr std::size_t kNeckSegmentCount = 2;

// Object representing the snake's head. This is what will collide with objects to pick them up,
// or with the edges and body segments to trigger a game over.
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
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{svanes::Circle2D{.radius = kRadius}});
    world.AddComponent<svanes::Timeline>(entity);
    world.AddComponent<svanes::Kinematic2D>(
        entity,
        svanes::Kinematic2D{.velocity_x = svanes::Cos(heading) * speed,
                            .velocity_y = svanes::Sin(heading) * speed});
}

// Callback to update the snake's heading as well as the body segment positions
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

// Object for a segment of the snake's body, with adjustable size
SnakeBody::SnakeBody(svanes::Registry &world, svanes::Vector2D position)
    : entity(world.CreateEntity()) {
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{.color = {60, 160, 80, 255},
                           .geometry = svanes::Circle2D{.radius = kRadius}});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{svanes::Circle2D{.radius = kRadius}});
}

// Helper for updating segment position to follow the head's path
void SnakeBody::SetPosition(svanes::Registry &world,
                            svanes::Vector2D position) {
    auto &transform = world.GetComponent<svanes::Transform>(entity);
    transform.x = position.x;
    transform.y = position.y;
}

// Updates the radius of a body segment
void SnakeBody::SetRadius(svanes::Registry &world, float radius) {
    if (!std::isfinite(radius) || radius <= 0.0F) {
        throw std::invalid_argument(
            "Snake body radius must be finite and positive.");
    }
    auto &shape = world.GetComponent<svanes::SolidShape>(entity);
    std::get<svanes::Circle2D>(shape.geometry).radius = radius;
    if (world.HasComponent<svanes::Collider2D>(entity)) {
        auto &collider = world.GetComponent<svanes::Collider2D>(entity);
        std::get<svanes::Circle2D>(collider.geometry).radius = radius;
    }
}

svanes::Entity SnakeBody::GetEntity() const { return entity; }

void Snake::Initialize(svanes::GameContext &context) {
    events = &context.events;
    // Create the snake with initial position and heading
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
    // Add on initial body segments
    for (std::size_t index = 0; index < kInitialBodyCount; ++index) {
        body.push_back(body_pool.Make(
            context.world,
            position -
                forward * (kBodySpacing * static_cast<float>(index + 1))));
        if (index < kNeckSegmentCount) {
            context.world.RemoveComponent<svanes::Collider2D>(
                body.back()->GetEntity());
        }
    }
    // Create colllidable reagions for edges
    InitializeEdges(context);
    const std::string font_path = svanes::AssetPath(
        "assets/nutritionallyAccurateSnake/fonts/consola.ttf");
    const auto font = context.fonts.LoadFont(font_path, 24.0F);
    if (font.id == 0) {
        throw std::runtime_error("Snake could not load its font from " +
                                 font_path);
    }
    // Placeholder collision handling
    collision_label = context.world.CreateEntity();
    context.world.AddComponent<svanes::TextLabel>(
        collision_label, svanes::TextLabel{.text = "oh noes",
                                           .position = {16.0F, 16.0F},
                                           .color = {255, 255, 255, 255},
                                           .font = font,
                                           .visible = false});
    context.events.Subscribe(
        std::make_unique<svanes::EventHandlerWrapper<svanes::CollisionEvent>>(
            [this,
             &world = context.world](const svanes::CollisionEvent &event) {
                HandleCollision(world, event);
            }));
}

void Snake::PhysicsUpdate(const svanes::PhysicsContext &physics) {
    head->PhysicsUpdate(physics);
    UpdateBody(physics.world);
    std::vector<svanes::Entity> colliders;
    physics.world.ForEach<svanes::Collider2D, svanes::Transform>(
        [&](svanes::Entity entity, const auto &, const auto &) {
            colliders.push_back(entity);
        });
    physics.world.GetComponent<svanes::TextLabel>(collision_label).visible =
        false;
    svanes::DetectEntityCollisions(physics.world, colliders, *events,
                                   physics.parallel_for);
    events->DispatchEvents();
}

// Moves body segments along the path made by the head
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

// Set the edges of the game space
void Snake::InitializeEdges(svanes::GameContext &context) {
    const float width = static_cast<float>(std::max(context.output_width, 1));
    const float height = static_cast<float>(std::max(context.output_height, 1));
    const auto bounds = context.camera.ScreenToWorld(
        {width * 0.5F, height * 0.5F, width, height});
    constexpr float thickness = 32.0F;
    const float x_offset = (bounds.width - thickness) * 0.5F;
    const float y_offset = (bounds.height - thickness) * 0.5F;
    const std::array<svanes::Rectangle2D, 4> rectangles{
        svanes::Rectangle2D{bounds.x - x_offset, bounds.y, thickness,
                            bounds.height},
        svanes::Rectangle2D{bounds.x + x_offset, bounds.y, thickness,
                            bounds.height},
        svanes::Rectangle2D{bounds.x, bounds.y - y_offset, bounds.width,
                            thickness},
        svanes::Rectangle2D{bounds.x, bounds.y + y_offset, bounds.width,
                            thickness}};
    for (std::size_t index = 0; index < edges.size(); ++index) {
        edges[index] = context.world.CreateEntity();
        context.world.AddComponent<svanes::Transform>(edges[index]);
        context.world.AddComponent<svanes::Collider2D>(
            edges[index], svanes::Collider2D{rectangles[index]});
        context.world.AddComponent<svanes::SolidShape>(
            edges[index], svanes::SolidShape{.color = {100, 100, 100, 255},
                                             .geometry = rectangles[index]});
    }
}

// Collision handler for game over and food pickup
void Snake::HandleCollision(svanes::Registry &world,
                            const svanes::CollisionEvent &event) {
    const auto a = event.collision.a;
    const auto b = event.collision.b;
    const auto head_entity = head->GetEntity();
    if (a != head_entity && b != head_entity) {
        return;
    }
    const auto other = a == head_entity ? b : a;
    if (std::find(edges.begin(), edges.end(), other) != edges.end() ||
        std::any_of(body.begin(), body.end(), [&](const auto &segment) {
            return segment->GetEntity() == other;
        })) {
        world.GetComponent<svanes::TextLabel>(collision_label).visible = true;
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

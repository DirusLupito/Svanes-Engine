#include "snake.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/collision_pass.hpp>
#include <svanes/deterministic_math.hpp>
#include <svanes/events/collision_event.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/geometry/convex_polygon_geometry.hpp>
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
#include <utility>

namespace nutritionally_accurate_snake {

constexpr float kMoveSpeed = 180.0F;
constexpr float kMaxTurnSpeed = std::numbers::pi_v<float>;
constexpr float kMinTurnSpeed = kMaxTurnSpeed / 6.0F;
constexpr float kTurnSpeedStep = kMaxTurnSpeed / 12.0F;
constexpr float kRadius = 19.2F;
constexpr float kBodySpacing = 19.2F;
constexpr std::size_t kInitialBodyCount = 3;
constexpr std::size_t kNeckSegmentCount = 2;
constexpr float kFoodRadius = 18.0F;
constexpr svanes::TicCount kFoodSpawnInterval = 5 * svanes::TicsPerSecond;
constexpr svanes::TicCount kDeathSegmentInterval = svanes::TicsPerSecond / 10;
constexpr float kWeightRadiusStep = 2.4F;
constexpr std::uint32_t kMaxDietBalance = 10;

static svanes::ConvexPolygon2D SnakeGeometry(float radius) {
    if (!std::isfinite(radius) || radius <= 0.0F) {
        throw std::invalid_argument(
            "Snake radius must be finite and positive.");
    }
    constexpr std::size_t vertex_count = 32;
    std::vector<svanes::Vector2D> vertices;
    vertices.reserve(vertex_count);
    for (std::size_t index = 0; index < vertex_count; ++index) {
        const float angle = 2.0F * std::numbers::pi_v<float> *
                            static_cast<float>(index) /
                            static_cast<float>(vertex_count);
        vertices.push_back(
            {kRadius * svanes::Cos(angle), radius * svanes::Sin(angle)});
    }
    return svanes::ConvexPolygon2D{std::move(vertices)};
}

static void SetSnakeRadius(svanes::Registry &world, svanes::Entity entity,
                           float radius) {
    auto geometry = SnakeGeometry(radius);
    world.GetComponent<svanes::SolidShape>(entity).geometry = geometry;
    if (world.HasComponent<svanes::Collider2D>(entity)) {
        world.GetComponent<svanes::Collider2D>(entity).geometry =
            std::move(geometry);
    }
}

// Object representing the snake's head. This is what will collide with objects to pick them up,
// or with the edges and body segments to trigger a game over.
SnakeHead::SnakeHead(svanes::Registry &world)
    : entity(world.CreateEntity()), turn_speed(kMaxTurnSpeed) {
    constexpr float heading = std::numbers::pi_v<float> / 6.0F;
    const float speed = svanes::PerSecondToPerTic(kMoveSpeed);
    auto geometry = SnakeGeometry(kRadius);

    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.rotation = heading});
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{.color = {80, 200, 100, 255}, .geometry = geometry});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{1});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{std::move(geometry)});
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
        svanes::PerSecondToPerTic(turn_speed) * static_cast<float>(delta_tics);
    transform.rotation = std::remainder(
        transform.rotation + std::clamp(difference, -max_turn, max_turn),
        2.0F * std::numbers::pi_v<float>);

    const float speed = svanes::PerSecondToPerTic(kMoveSpeed);
    auto &motion = physics.world.GetComponent<svanes::Kinematic2D>(entity);
    motion.velocity_x = svanes::Cos(transform.rotation) * speed;
    motion.velocity_y = svanes::Sin(transform.rotation) * speed;
}

void SnakeHead::SetRadius(svanes::Registry &world, float radius) {
    SetSnakeRadius(world, entity, radius);
}

void SnakeHead::AdjustTurnSpeed(float adjustment) {
    turn_speed =
        std::clamp(turn_speed + adjustment, kMinTurnSpeed, kMaxTurnSpeed);
}

svanes::Entity SnakeHead::GetEntity() const { return entity; }

// Object for a segment of the snake's body, with adjustable size
SnakeBody::SnakeBody(svanes::Registry &world, svanes::Vector2D position,
                     float radius)
    : entity(world.CreateEntity()) {
    auto geometry = SnakeGeometry(radius);
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{.color = {60, 160, 80, 255}, .geometry = geometry});
    world.AddComponent<svanes::Collider2D>(
        entity, svanes::Collider2D{std::move(geometry)});
}

// Helper for updating segment position to follow the head's path
void SnakeBody::SetPosition(svanes::Registry &world, svanes::Vector2D position,
                            float heading) {
    auto &transform = world.GetComponent<svanes::Transform>(entity);
    transform.x = position.x;
    transform.y = position.y;
    transform.rotation = heading;
}

// Updates the radius of a body segment
void SnakeBody::SetRadius(svanes::Registry &world, float radius) {
    SetSnakeRadius(world, entity, radius);
}

svanes::Entity SnakeBody::GetEntity() const { return entity; }

void Snake::ResetSnake(svanes::Registry &world) {
    if (head) {
        world.DestroyEntity(head->GetEntity());
        head.reset();
    }
    for (const auto &segment : body) {
        world.DestroyEntity(segment->GetEntity());
    }
    body.clear();
    path.clear();
    for (const auto pellet : pellets) {
        world.DestroyEntity(pellet);
    }
    pellets.clear();
    food_spawn_tics = 0;
    death_animation_tics = 0;
    score = 0;
    weight = 0.0F;
    diet_balance = 0;
    // Create the snake with initial position and heading
    head = head_pool.Make(world);
    const auto &transform =
        world.GetComponent<svanes::Transform>(head->GetEntity());
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
            world,
            position - forward * (kBodySpacing * static_cast<float>(index + 1)),
            kRadius));
        if (index < kNeckSegmentCount) {
            world.RemoveComponent<svanes::Collider2D>(body.back()->GetEntity());
        }
    }
    UpdateBody(world);
    starting = false;
    game_over = false;
}

void Snake::Initialize(svanes::GameContext &context) {
    events = &context.events;
    // Create colllidable reagions for edges
    InitializeEdges(context);
    const std::string font_path = svanes::AssetPath(
        "assets/nutritionallyAccurateSnake/fonts/consola.ttf");
    const auto font = context.fonts.LoadFont(font_path, 24.0F);
    if (font.id == 0) {
        throw std::runtime_error("Snake could not load its font from " +
                                 font_path);
    }

    score_label = context.world.CreateEntity();
    context.world.AddComponent<svanes::TextLabel>(
        score_label, svanes::TextLabel{.text = "Score: 0",
                                       .position = {16.0F, 16.0F},
                                       .color = {255, 255, 255, 255},
                                       .font = font,
                                       .visible = false});

    explanation_label = context.world.CreateEntity();
    context.world.AddComponent<svanes::TextLabel>(
        explanation_label,
        svanes::TextLabel{
            .text = "Burgers and pizza give you more points, but will cause "
                    "you to get wider and turn slower. Healthy salads and "
                    "apples will remedy these effects.\n\nThe unhealthier you "
                    "eat, the more points you get from unhealthy food, but the "
                    "harder it is to mitigate the negative effects of your "
                    "gluttonous habits.\n\nGood luck!",
            .color = {255, 255, 255, 255},
            .font = font,
            .alignment = svanes::TextAlignment::TopCenter,
            .visible = false});
    context.world.AddComponent<svanes::ClipRect>(explanation_label);

    const std::array<const char *, 3> captions{"Nutritionally Accurate Snake",
                                               "Start", "Quit"};
    for (std::size_t index = 0; index < captions.size(); ++index) {
        menu_shapes[index] = context.world.CreateEntity();
        context.world.AddComponent<svanes::Transform>(menu_shapes[index]);
        context.world.AddComponent<svanes::SolidShape>(
            menu_shapes[index],
            svanes::SolidShape{.color = {0, 0, 0, 0},
                               .geometry = svanes::Rectangle2D{
                                   .width = 1.0F, .height = 1.0F}});
        context.world.AddComponent<svanes::ZOrder>(
            menu_shapes[index], svanes::ZOrder{index == 0 ? 10 : 11});
        menu_labels[index] = context.world.CreateEntity();
        context.world.AddComponent<svanes::TextLabel>(
            menu_labels[index],
            svanes::TextLabel{.text = captions[index],
                              .color = {255, 255, 255, 255},
                              .font = font,
                              .alignment = svanes::TextAlignment::Center,
                              .visible = false});
    }
    context.events.Subscribe(
        std::make_unique<svanes::EventHandlerWrapper<svanes::CollisionEvent>>(
            [this,
             &world = context.world](const svanes::CollisionEvent &event) {
                HandleCollision(world, event);
            }));
}

void Snake::PhysicsUpdate(const svanes::PhysicsContext &physics) {
    if (starting || game_over) {
        return;
    }
    head->PhysicsUpdate(physics);
    UpdateBody(physics.world);
    std::vector<svanes::Entity> colliders;
    physics.world.ForEach<svanes::Collider2D, svanes::Transform>(
        [&](svanes::Entity entity, const auto &, const auto &) {
            colliders.push_back(entity);
        });
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
                    path[point] + direction * ((target - distance) / length),
                    std::atan2(-direction.y, -direction.x));
                break;
            }
            distance += length;
            ++point;
        }
        if (point + 1 == path.size()) {
            const auto direction = path[path.size() - 2] - path.back();
            body[index]->SetPosition(world, path.back(),
                                     std::atan2(direction.y, direction.x));
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
    food_spawn_area = {bounds.x, bounds.y,
                       bounds.width - 2.0F * (thickness + kFoodRadius),
                       bounds.height - 2.0F * (thickness + kFoodRadius)};
    if (food_spawn_area.width <= 0.0F || food_spawn_area.height <= 0.0F) {
        throw std::runtime_error("Snake arena is too small to spawn food.");
    }
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

void Snake::SpawnFood(svanes::Registry &world) {
    constexpr std::array<svanes::Color, 4> colors{
        svanes::Color{240, 160, 50, 255}, svanes::Color{230, 60, 60, 255},
        svanes::Color{120, 210, 255, 255}, svanes::Color{40, 70, 180, 255}};
    std::uniform_int_distribution<std::size_t> type_index{0, colors.size() - 1};
    const auto type = static_cast<PelletType>(type_index(random));
    std::uniform_real_distribution<float> x{
        food_spawn_area.x - food_spawn_area.width * 0.5F,
        food_spawn_area.x + food_spawn_area.width * 0.5F};
    std::uniform_real_distribution<float> y{
        food_spawn_area.y - food_spawn_area.height * 0.5F,
        food_spawn_area.y + food_spawn_area.height * 0.5F};
    const auto entity = world.CreateEntity();
    world.AddComponent<PelletType>(entity, type);
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = x(random), .y = y(random)});
    const svanes::Circle2D geometry{.radius = kFoodRadius};
    world.AddComponent<svanes::SolidShape>(
        entity,
        svanes::SolidShape{.color = colors[static_cast<std::size_t>(type)],
                           .geometry = geometry});
    world.AddComponent<svanes::Collider2D>(entity,
                                           svanes::Collider2D{geometry});
    pellets.push_back(entity);
}

// Collision handler for game over and food pickup
void Snake::HandleCollision(svanes::Registry &world,
                            const svanes::CollisionEvent &event) {
    if (starting || game_over) {
        return;
    }
    const auto a = event.collision.a;
    const auto b = event.collision.b;
    const auto head_entity = head->GetEntity();
    if (a != head_entity && b != head_entity) {
        return;
    }
    const auto other = a == head_entity ? b : a;
    const auto pellet = std::find(pellets.begin(), pellets.end(), other);
    if (pellet != pellets.end()) {
        const auto previous_weight = weight;
        const auto type = world.GetComponent<PelletType>(other);
        const bool unhealthy =
            type == PelletType::Orange || type == PelletType::Red;
        float balance = 0.5F;
        if (diet_balance <= 2) {
            balance = 0.0F;
        } else if (diet_balance >= 8) {
            balance = 1.0F;
        }
        const float score_multiplier =
            unhealthy ? 1.0F + balance : 1.0F - 0.5F * balance;
        const float penalty_multiplier = 0.75F + 1.25F * balance;
        const float recovery_multiplier = 1.25F - 0.75F * balance;
        score += static_cast<std::uint64_t>(
            std::lround((unhealthy ? 25.0F : 10.0F) * score_multiplier));
        switch (type) {
        case PelletType::Orange:
            weight += penalty_multiplier;
            break;
        case PelletType::Red:
            head->AdjustTurnSpeed(-kTurnSpeedStep * penalty_multiplier);
            break;
        case PelletType::LightBlue:
            weight = std::max(0.0F, weight - recovery_multiplier);
            break;
        case PelletType::DarkBlue:
            head->AdjustTurnSpeed(kTurnSpeedStep * recovery_multiplier);
            break;
        }
        if (unhealthy) {
            diet_balance = std::min(diet_balance + 1, kMaxDietBalance);
        } else if (diet_balance > 0) {
            --diet_balance;
        }
        world.DestroyEntity(other);
        pellets.erase(pellet);
        const float radius = kRadius + weight * kWeightRadiusStep;
        if (weight != previous_weight) {
            head->SetRadius(world, radius);
            for (const auto &segment : body) {
                segment->SetRadius(world, radius);
            }
        }
        body.push_back(body_pool.Make(world, path.back(), radius));
        UpdateBody(world);
        return;
    }
    if (std::find(edges.begin(), edges.end(), other) != edges.end() ||
        std::any_of(body.begin(), body.end(), [&](const auto &segment) {
            return segment->GetEntity() == other;
        })) {
        game_over = true;
        death_animation_tics = 0;
        world.GetComponent<svanes::TextLabel>(menu_labels[0]).text =
            "Game Over\nScore: " + std::to_string(score);
        world.GetComponent<svanes::TextLabel>(menu_labels[1]).text = "Restart";
        world.GetComponent<svanes::Timeline>(head_entity).Pause();
    }
}

void Snake::UpdateMenuScreen(const svanes::FrameContext &frame) {
    const auto viewport = frame.camera.Viewport();
    const float button_y = starting
                               ? viewport.y + viewport.height * 0.5F - 64.0F
                               : viewport.y + 40.0F;
    const std::array<svanes::Rectangle2D, 3> areas{
        viewport,
        svanes::Rectangle2D{viewport.x - 130.0F, button_y, 220.0F, 64.0F},
        svanes::Rectangle2D{viewport.x + 130.0F, button_y, 220.0F, 64.0F}};
    if ((starting || game_over) &&
        frame.input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        const auto mouse = frame.input.MousePosition();
        for (std::size_t index = 1; index < areas.size(); ++index) {
            const auto &area = areas[index];
            if (mouse.x >= area.x - area.width * 0.5F &&
                mouse.x <= area.x + area.width * 0.5F &&
                mouse.y >= area.y - area.height * 0.5F &&
                mouse.y <= area.y + area.height * 0.5F) {
                if (index == 1) {
                    ResetSnake(frame.world);
                } else {
                    quit = true;
                }
                break;
            }
        }
    }
    for (std::size_t index = 0; index < areas.size(); ++index) {
        const auto area = frame.camera.ScreenToWorld(areas[index]);
        auto &transform =
            frame.world.GetComponent<svanes::Transform>(menu_shapes[index]);
        transform.x = area.x;
        transform.y = area.y;
        auto &shape =
            frame.world.GetComponent<svanes::SolidShape>(menu_shapes[index]);
        shape.geometry =
            svanes::Rectangle2D{.width = area.width, .height = area.height};
        shape.color = index == 0 ? svanes::Color{0, 0, 0, 96}
                                 : svanes::Color{70, 70, 70, 255};
        if (index == 0 && starting) {
            shape.color.alpha = 255;
        }
        if (!starting && !game_over) {
            shape.color.alpha = 0;
        }
        auto &label =
            frame.world.GetComponent<svanes::TextLabel>(menu_labels[index]);
        label.position =
            index == 0
                ? svanes::Vector2D{viewport.width * 0.5F,
                                   starting ? viewport.height * 0.15F
                                            : viewport.height * 0.5F - 80.0F}
                : svanes::Vector2D{
                      areas[index].x - viewport.x + viewport.width * 0.5F,
                      areas[index].y - viewport.y + viewport.height * 0.5F};
        label.visible = starting || game_over;
    }
    auto &explanation =
        frame.world.GetComponent<svanes::TextLabel>(explanation_label);
    explanation.position = {viewport.width * 0.5F, viewport.height * 0.25F};
    explanation.wrap_width = static_cast<std::int32_t>(
        std::clamp(viewport.width - 64.0F, 1.0F, 800.0F));
    const float explanation_height =
        std::max(0.0F, viewport.height - 120.0F - explanation.position.y);
    frame.world.GetComponent<svanes::ClipRect>(explanation_label).area =
        svanes::Rectangle2D{explanation.position.x,
                            explanation.position.y + explanation_height * 0.5F,
                            static_cast<float>(explanation.wrap_width),
                            explanation_height};
    explanation.visible = starting;
}

void Snake::Update(const svanes::FrameContext &frame) {
    quit = frame.input.WasPressed(svanes::Key::Escape);
    if (game_over && head) {
        death_animation_tics += frame.real_delta_tics;
        while (death_animation_tics >= kDeathSegmentInterval && head) {
            death_animation_tics -= kDeathSegmentInterval;
            if (!body.empty()) {
                frame.world.DestroyEntity(body.back()->GetEntity());
                body.pop_back();
            } else {
                frame.world.DestroyEntity(head->GetEntity());
                head.reset();
                path.clear();
            }
        }
    }
    if (!starting && !game_over) {
        food_spawn_tics += frame.real_delta_tics;
        if (food_spawn_tics >= kFoodSpawnInterval) {
            food_spawn_tics %= kFoodSpawnInterval;
            SpawnFood(frame.world);
        }
    }
    UpdateMenuScreen(frame);
    auto &score_display =
        frame.world.GetComponent<svanes::TextLabel>(score_label);
    score_display.text = "Score: " + std::to_string(score);
    score_display.visible = !starting;
}

bool Snake::ShouldQuit() const { return quit; }

svanes::TicCount Snake::GetFrameIntervalTics() const {
    return svanes::TicsPerSecond / 60;
}

}

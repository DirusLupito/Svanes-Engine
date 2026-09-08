#include "goose.hpp"

#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t kFrameWidth = 29;
constexpr std::int32_t kFrameHeight = 27;
constexpr std::int32_t kWalkFrameCount = 4;
constexpr float kSpriteScale = 1.5F;
constexpr float kBodyWidth = static_cast<float>(kFrameWidth) * kSpriteScale;
constexpr float kBodyHeight = static_cast<float>(kFrameHeight) * kSpriteScale;
constexpr float kBulletSpeed = 1200.0F;
constexpr float kFireInterval = 0.15F;
constexpr svanes::Color kBulletColor{255, 230, 120, 255};
constexpr float kKnockbackLockSeconds = 0.25F;
constexpr float kInvincibleSeconds = 0.8F;
constexpr float kWalkSecondsPerFrame = 0.1F;
constexpr float kFlySecondsPerFrame = 0.06F;

}

void Goose::Spawn(svanes::GameContext& context, float x, float y)
{
    if (spawned) {
        throw std::logic_error("Goose::Spawn called twice on the same goose.");
    }

    idle_texture = context.assets.LoadTexture(std::string{ERIK_GAME_ASSETS_DIR} + "/goose.png");
    walk_texture = context.assets.LoadTexture(std::string{ERIK_GAME_ASSETS_DIR} + "/goose_walk.png");

    const svanes::Rectangle2D body{
        .width = kBodyWidth,
        .height = kBodyHeight,
    };

    entity = context.world.CreateEntity();
    context.world.AddComponent<svanes::Transform>(entity, svanes::Transform{
        .x = x,
        .y = y,
    });
    context.world.AddComponent<svanes::Sprite>(entity, svanes::Sprite{
        .texture = idle_texture,
        .geometry = body,
    });
    context.world.AddComponent<svanes::Kinematic2D>(entity);
    context.world.AddComponent<svanes::Gravity>(entity);
    context.world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});

    fly_time_remaining = max_fly_seconds;
    spawned = true;
}

void Goose::Update(const svanes::FrameContext& frame, const GooseIntent& intent)
{
    if (!spawned) {
        throw std::logic_error("Goose::Update called before Goose::Spawn.");
    }

    time_in_state += frame.delta_seconds;
    knockback_timer = std::max(knockback_timer - frame.delta_seconds, 0.0F);
    invincible_timer = std::max(invincible_timer - frame.delta_seconds, 0.0F);
    dash_timer = std::max(dash_timer - frame.delta_seconds, 0.0F);
    dash_cooldown = std::max(dash_cooldown - frame.delta_seconds, 0.0F);

    ResolveCollisions(frame.world);

    if (grounded) {
        fly_time_remaining = max_fly_seconds;
    }

    svanes::Kinematic2D& motion = frame.world.GetComponent<svanes::Kinematic2D>(entity);

    bool flying = false;

    if (knockback_timer <= 0.0F) {
        if (dash_timer <= 0.0F) {
            motion.velocity_x = intent.move.x * speed;
        }

        if (intent.dash != 0.0F && dash_cooldown <= 0.0F) {
            motion.velocity_x = intent.dash * dash_speed;
            dash_timer = dash_seconds;
            dash_cooldown = dash_cooldown_seconds;
        }

        if (intent.jump && grounded) {
            motion.velocity_y = -jump_speed;
            grounded = false;
        }

        flying = intent.jump && !grounded && fly_time_remaining > 0.0F;

        if (flying) {
            motion.velocity_y = std::min(motion.velocity_y, -fly_rise_speed);
            fly_time_remaining = std::max(fly_time_remaining - frame.delta_seconds, 0.0F);
        }
    }

    motion.velocity_y = std::min(motion.velocity_y, max_fall_speed);

    fire_cooldown = std::max(fire_cooldown - frame.delta_seconds, 0.0F);

    if (intent.fire && fire_cooldown <= 0.0F) {
        const svanes::Transform& transform = frame.world.GetComponent<svanes::Transform>(entity);
        const svanes::Vector2D origin{transform.x, transform.y};
        const svanes::Vector2D direction = intent.aim_point - origin;

        if (direction.x != 0.0F || direction.y != 0.0F) {
            SpawnBullet(frame.world, entity, origin, direction, kBulletSpeed, kBulletColor);
            fire_cooldown = kFireInterval;
        }
    }

    GooseState next = GooseState::Idle;
    if (flying) {
        next = GooseState::Flying;
    } else if (intent.move.x != 0.0F) {
        next = GooseState::Walking;
    }

    SetState(frame.world, next);
}

void Goose::ResolveCollisions(svanes::Registry& world)
{
    grounded = false;

    svanes::Transform& transform = world.GetComponent<svanes::Transform>(entity);
    svanes::Kinematic2D& motion = world.GetComponent<svanes::Kinematic2D>(entity);
    const svanes::Geometry2D body = world.GetComponent<svanes::Collider2D>(entity).geometry;

    world.ForEach<svanes::Transform, svanes::Collider2D, Solid>(
        [&](svanes::Entity other, svanes::Transform& other_transform, svanes::Collider2D& other_collider, Solid&) {
            if (other == entity) {
                return;
            }

            const std::vector<svanes::Collision2D> collisions = svanes::DetectCollisions(
                body, transform, other_collider.geometry, other_transform
            );

            for (const svanes::Collision2D& collision : collisions) {
                transform.x += collision.normal.x * collision.penetration_depth;
                transform.y += collision.normal.y * collision.penetration_depth;

                if (collision.normal.y < 0.0F) {
                    grounded = true;
                    motion.velocity_y = std::min(motion.velocity_y, 0.0F);
                }

                if (collision.normal.y > 0.0F) {
                    motion.velocity_y = std::max(motion.velocity_y, 0.0F);
                }

                if (collision.normal.x != 0.0F) {
                    motion.velocity_x = 0.0F;
                }
            }
        }
    );
}

void Goose::SetState(svanes::Registry& world, GooseState next)
{
    if (state == next) {
        return;
    }

    state = next;
    time_in_state = 0.0F;

    svanes::Sprite& sprite = world.GetComponent<svanes::Sprite>(entity);

    switch (next) {
    case GooseState::Idle:
        sprite.texture = idle_texture;
        sprite.source = std::nullopt;
        world.RemoveComponent<svanes::SpriteAnimation>(entity);
        break;
    case GooseState::Walking:
        sprite.texture = walk_texture;
        world.AddComponent<svanes::SpriteAnimation>(entity, svanes::SpriteAnimation{
            .frame_width = kFrameWidth,
            .frame_height = kFrameHeight,
            .frame_count = kWalkFrameCount,
            .seconds_per_frame = kWalkSecondsPerFrame,
        });
        break;
    case GooseState::Flying:
        sprite.texture = walk_texture;
        world.AddComponent<svanes::SpriteAnimation>(entity, svanes::SpriteAnimation{
            .frame_width = kFrameWidth,
            .frame_height = kFrameHeight,
            .frame_count = kWalkFrameCount,
            .seconds_per_frame = kFlySecondsPerFrame,
        });
        break;
    }
}

void Goose::ApplyKnockback(svanes::Registry& world, svanes::Vector2D direction, float speed)
{
    if (!spawned) {
        throw std::logic_error("Goose::ApplyKnockback called before Goose::Spawn.");
    }

    if (invincible_timer > 0.0F) {
        return;
    }

    const float length = std::hypot(direction.x, direction.y);
    if (!std::isfinite(length) || length <= 0.0F) {
        throw std::invalid_argument("Goose::ApplyKnockback requires a finite and nonzero direction.");
    }

    if (!std::isfinite(speed) || speed <= 0.0F) {
        throw std::invalid_argument("Goose::ApplyKnockback requires a finite and positive speed.");
    }

    const svanes::Vector2D velocity = direction / length * speed;

    svanes::Kinematic2D& motion = world.GetComponent<svanes::Kinematic2D>(entity);
    motion.velocity_x = velocity.x;
    motion.velocity_y = velocity.y;

    knockback_timer = kKnockbackLockSeconds;
    invincible_timer = kInvincibleSeconds;
    dash_timer = 0.0F;
    grounded = false;
}

svanes::Entity Goose::GetEntity() const
{
    return entity;
}

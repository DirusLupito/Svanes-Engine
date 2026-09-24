#include "goose.hpp"

#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry/geometry.hpp>
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
const svanes::TicCount kFireInterval = svanes::SecondsToTics(0.15);
constexpr svanes::Color kBulletColor{255, 230, 120, 255};
const svanes::TicCount kKnockbackLockTics = svanes::SecondsToTics(0.25);
const svanes::TicCount kInvincibleTics = svanes::SecondsToTics(0.8);
constexpr float kWalkSecondsPerFrame = 0.1F;
constexpr float kFlySecondsPerFrame = 0.06F;

/**
 * Subtracts elapsed simulation time from a countdown without unsigned underflow.
 * @param remaining The countdown to advance.
 * @param delta_tics The elapsed local tics.
 */
void AdvanceCountdown(svanes::TicCount& remaining, svanes::TicCount delta_tics)
{
    remaining -= std::min(remaining, delta_tics);
}

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
    context.world.AddComponent<svanes::Timeline>(entity);
    // TASK 3, physics: these two components are what put the goose under the
    // engine's physics. Kinematic2D holds its velocity and acceleration, and
    // Gravity opts it into the world gravity vector set in ErikGame::Initialize
    context.world.AddComponent<svanes::Kinematic2D>(entity);
    context.world.AddComponent<svanes::Gravity>(entity);
    context.world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});

    fly_time_remaining = max_fly_tics;
    spawned = true;
}

void Goose::Update(const svanes::FrameContext& frame, const GooseIntent& intent)
{
    Advance(frame.world, intent, frame.real_delta_tics);
}

void Goose::Advance(svanes::Registry& world, const GooseIntent& intent,
                    svanes::TicCount delta_tics)
{
    if (!spawned) {
        throw std::logic_error("Goose::Advance called before Goose::Spawn.");
    }
    if (delta_tics == 0) {
        return;
    }
    AdvanceCountdown(knockback_timer, delta_tics);
    AdvanceCountdown(invincible_timer, delta_tics);
    AdvanceCountdown(dash_timer, delta_tics);
    AdvanceCountdown(dash_cooldown, delta_tics);

    ResolveCollisions(world);

    if (grounded) {
        fly_time_remaining = max_fly_tics;
    }

    svanes::Kinematic2D& motion = world.GetComponent<svanes::Kinematic2D>(entity);
    motion.acceleration_y = 0.0F;

    bool flying = false;

    if (knockback_timer == 0) {
        if (dash_timer == 0) {
            motion.velocity_x = svanes::PerSecondToPerTic(intent.move.x * speed);
        }

        if (intent.dash != 0.0F && dash_cooldown == 0) {
            motion.velocity_x = svanes::PerSecondToPerTic(intent.dash * dash_speed);
            dash_timer = dash_tics;
            dash_cooldown = dash_cooldown_tics;
        }

        if (intent.jump && grounded) {
            motion.velocity_y = svanes::PerSecondToPerTic(-jump_speed);
            grounded = false;
        }

        flying = intent.jump && !grounded && fly_time_remaining > 0;

        if (flying) {
            if (motion.velocity_y > svanes::PerSecondToPerTic(-fly_rise_speed)) {
                motion.acceleration_y = svanes::PerSecondSquaredToPerTicSquared(-fly_rise_acceleration);
            }

            AdvanceCountdown(fly_time_remaining, delta_tics);
        }
    }

    motion.velocity_y = std::min(
        motion.velocity_y, svanes::PerSecondToPerTic(max_fall_speed));

    AdvanceCountdown(fire_cooldown, delta_tics);

    if (intent.fire && fire_cooldown == 0) {
        const svanes::Transform& transform = world.GetComponent<svanes::Transform>(entity);
        const svanes::Vector2D origin{transform.x, transform.y};
        const svanes::Vector2D direction = intent.aim_point - origin;

        if (direction.x != 0.0F || direction.y != 0.0F) {
            SpawnBullet(world, entity, origin, direction, kBulletSpeed, kBulletColor);
            fire_cooldown = kFireInterval;
        }
    }

    GooseState next = GooseState::Idle;
    if (flying) {
        next = GooseState::Flying;
    } else if (intent.move.x != 0.0F) {
        next = GooseState::Walking;
    }

    SetState(world, next);
}

// TASK 5, collision response: checks the goose against every Solid entity and
// responds to each overlap by pushing the goose back out along the collision
// normal, then killing the velocity that drove it into the surface. Landing on
// something is read off the same normals, so grounded is recalculated here rather
// than tracked separately
void Goose::ResolveCollisions(svanes::Registry& world)
{
    grounded = false;

    svanes::Transform& transform = world.GetComponent<svanes::Transform>(entity);
    svanes::Kinematic2D& motion = world.GetComponent<svanes::Kinematic2D>(entity);
    const svanes::Geometry2D body = world.GetComponent<svanes::Collider2D>(entity).geometry;

    std::vector<svanes::Entity> solids;
    world.ForEach<svanes::Transform, svanes::Collider2D, Solid>(
        [&](svanes::Entity other, const svanes::Transform&, const svanes::Collider2D&, const Solid&) {
            if (other != entity) {
                solids.push_back(other);
            }
        }
    );
    // Resolve corners in entity order so hash table iteration cannot change the result.
    std::sort(solids.begin(), solids.end());
    for (const svanes::Entity other : solids) {
        const auto& other_transform = world.GetComponent<svanes::Transform>(other);
        const auto& other_collider = world.GetComponent<svanes::Collider2D>(other);

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
}

GooseSnapshot Goose::Capture(const svanes::Registry& world) const
{
    if (!spawned) {
        throw std::logic_error("Goose::Capture called before Goose::Spawn.");
    }
    std::optional<svanes::SpriteAnimation> animation;
    if (world.HasComponent<svanes::SpriteAnimation>(entity)) {
        animation = world.GetComponent<svanes::SpriteAnimation>(entity);
    }
    return {
        world.GetComponent<svanes::Transform>(entity),
        world.GetComponent<svanes::Kinematic2D>(entity),
        world.GetComponent<svanes::Timeline>(entity),
        world.GetComponent<svanes::Sprite>(entity),
        animation, state, grounded, fly_time_remaining, dash_timer,
        dash_cooldown, fire_cooldown, knockback_timer, invincible_timer,
    };
}

void Goose::Restore(svanes::Registry& world, const GooseSnapshot& snapshot)
{
    if (!spawned) {
        throw std::logic_error("Goose::Restore called before Goose::Spawn.");
    }
    world.GetComponent<svanes::Transform>(entity) = snapshot.transform;
    world.GetComponent<svanes::Kinematic2D>(entity) = snapshot.motion;
    world.GetComponent<svanes::Timeline>(entity) = snapshot.timeline;
    world.GetComponent<svanes::Sprite>(entity) = snapshot.sprite;
    if (snapshot.animation) {
        world.AddComponent<svanes::SpriteAnimation>(entity, *snapshot.animation);
    } else {
        world.RemoveComponent<svanes::SpriteAnimation>(entity);
    }
    state = snapshot.state;
    grounded = snapshot.grounded;
    fly_time_remaining = snapshot.fly_time_remaining;
    dash_timer = snapshot.dash_timer;
    dash_cooldown = snapshot.dash_cooldown;
    fire_cooldown = snapshot.fire_cooldown;
    knockback_timer = snapshot.knockback_timer;
    invincible_timer = snapshot.invincible_timer;
}

void Goose::SetState(svanes::Registry& world, GooseState next)
{
    if (state == next) {
        return;
    }

    state = next;

    svanes::Sprite& sprite = world.GetComponent<svanes::Sprite>(entity);

    switch (next) {
    case GooseState::Idle:
        sprite.texture = idle_texture;
        sprite.source = std::nullopt;
        world.RemoveComponent<svanes::SpriteAnimation>(entity);
        break;
    case GooseState::Walking:
        sprite.texture = walk_texture;
        sprite.source = svanes::Rectangle2D{
            kFrameWidth * 0.5F, kFrameHeight * 0.5F,
            static_cast<float>(kFrameWidth), static_cast<float>(kFrameHeight)};
        world.AddComponent<svanes::SpriteAnimation>(entity, svanes::SpriteAnimation{
            .frame_width = kFrameWidth,
            .frame_height = kFrameHeight,
            .frame_count = kWalkFrameCount,
            .tics_per_frame = svanes::SecondsToTics(kWalkSecondsPerFrame),
        });
        break;
    case GooseState::Flying:
        sprite.texture = walk_texture;
        sprite.source = svanes::Rectangle2D{
            kFrameWidth * 0.5F, kFrameHeight * 0.5F,
            static_cast<float>(kFrameWidth), static_cast<float>(kFrameHeight)};
        world.AddComponent<svanes::SpriteAnimation>(entity, svanes::SpriteAnimation{
            .frame_width = kFrameWidth,
            .frame_height = kFrameHeight,
            .frame_count = kWalkFrameCount,
            .tics_per_frame = svanes::SecondsToTics(kFlySecondsPerFrame),
        });
        break;
    }
}

void Goose::ApplyKnockback(svanes::Registry& world, svanes::Vector2D direction, float speed)
{
    if (!spawned) {
        throw std::logic_error("Goose::ApplyKnockback called before Goose::Spawn.");
    }

    if (invincible_timer > 0) {
        return;
    }

    const float length = std::hypot(direction.x, direction.y);
    if (!std::isfinite(length) || length <= 0.0F) {
        throw std::invalid_argument("Goose::ApplyKnockback requires a finite and nonzero direction.");
    }

    if (!std::isfinite(speed) || speed <= 0.0F) {
        throw std::invalid_argument("Goose::ApplyKnockback requires a finite and positive speed.");
    }

    const svanes::Vector2D velocity = direction / length *
        svanes::PerSecondToPerTic(speed);

    svanes::Kinematic2D& motion = world.GetComponent<svanes::Kinematic2D>(entity);
    motion.velocity_x = velocity.x;
    motion.velocity_y = velocity.y;

    knockback_timer = kKnockbackLockTics;
    invincible_timer = kInvincibleTics;
    dash_timer = 0;
    grounded = false;
}

svanes::Entity Goose::GetEntity() const
{
    return entity;
}

#include "goose.hpp"

#include "arena.hpp"
#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

constexpr float kWalkSpeed = 300.0F;
constexpr float kJumpSpeed = 900.0F;
constexpr float kMaxFallSpeed = 1500.0F;
constexpr float kFlyRiseSpeed = 800.0F;
constexpr float kFlyRiseAcceleration = 4600.0F;
constexpr float kDashSpeed = 1400.0F;
constexpr float kBulletSpeed = 1200.0F;

// flight is a budget rather than a cooldown: it drains while flying and
// refills to kMaxFlyTics on landing, so the goose cannot hover forever
const svanes::TicCount kMaxFlyTics = svanes::SecondsToTics(2.0);

const svanes::TicCount kDashTics = svanes::SecondsToTics(0.15);
const svanes::TicCount kDashCooldownTics = svanes::SecondsToTics(1.0);
const svanes::TicCount kFireInterval = svanes::SecondsToTics(0.15);
const svanes::TicCount kKnockbackLockTics = svanes::SecondsToTics(0.25);
const svanes::TicCount kInvincibleTics = svanes::SecondsToTics(0.8);
const svanes::TicCount kRespawnTics = svanes::SecondsToTics(3.0);

/**
 * Subtracts elapsed time from a countdown without unsigned underflow.
 *
 * @param remaining The countdown to advance.
 * @param delta_tics The elapsed local tics.
 */
void AdvanceCountdown(svanes::TicCount &remaining,
                      svanes::TicCount delta_tics) {
    remaining -= std::min(remaining, delta_tics);
}

} // namespace

Goose::Goose(svanes::Registry &world, svanes::Entity clock,
             svanes::Vector2D position)
    : entity(world.CreateEntity()), spawn(position), health(GooseMaxHealth),
      fly_time_remaining(kMaxFlyTics) {
    const svanes::Rectangle2D body{.width = GooseWidth,
                                   .height = GooseHeight};

    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::Timeline>(entity, svanes::Timeline{clock});
    // TASK 3, physics: these two components are what put the goose under the
    // engine's physics. Kinematic2D holds its velocity and acceleration, and
    // Gravity opts it into the world gravity vector set in GooseServer::Initialize
    world.AddComponent<svanes::Kinematic2D>(entity);
    world.AddComponent<svanes::Gravity>(entity);
    world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
}

void Goose::Advance(svanes::Registry &world, const GooseIntent &intent) {
    const svanes::TicCount delta_tics =
        world.GetComponent<svanes::Timeline>(entity).GetDeltaTics();
    if (delta_tics == 0) {
        return;
    }

    AdvanceCountdown(knockback_timer, delta_tics);
    AdvanceCountdown(invincible_timer, delta_tics);
    AdvanceCountdown(dash_timer, delta_tics);
    AdvanceCountdown(dash_cooldown, delta_tics);
    AdvanceCountdown(fire_cooldown, delta_tics);

    ResolveCollisions(world);

    if (grounded) {
        fly_time_remaining = kMaxFlyTics;
    }

    svanes::Kinematic2D &motion =
        world.GetComponent<svanes::Kinematic2D>(entity);
    motion.acceleration_y = 0.0F;

    if (IsDead()) {
        motion.velocity_x = 0.0F;
        state = GooseState::Idle;
        AdvanceCountdown(respawn_timer, delta_tics);
        if (respawn_timer == 0) {
            Respawn(world);
        }
        return;
    }

    bool flying = false;

    if (knockback_timer == 0) {
        if (dash_timer == 0) {
            motion.velocity_x = svanes::PerSecondToPerTic(intent.move * kWalkSpeed);
        }

        if (intent.dash != 0.0F && dash_cooldown == 0) {
            motion.velocity_x =
                svanes::PerSecondToPerTic(intent.dash * kDashSpeed);
            dash_timer = kDashTics;
            dash_cooldown = kDashCooldownTics;
        }

        if (intent.jump && grounded) {
            motion.velocity_y = svanes::PerSecondToPerTic(-kJumpSpeed);
            grounded = false;
        }

        flying = intent.jump && !grounded && fly_time_remaining > 0;

        if (flying) {
            if (motion.velocity_y > svanes::PerSecondToPerTic(-kFlyRiseSpeed)) {
                motion.acceleration_y =
                    svanes::PerSecondSquaredToPerTicSquared(-kFlyRiseAcceleration);
            }

            AdvanceCountdown(fly_time_remaining, delta_tics);
        }
    }

    motion.velocity_y =
        std::min(motion.velocity_y, svanes::PerSecondToPerTic(kMaxFallSpeed));

    const bool has_aim = intent.aim.x != 0.0F || intent.aim.y != 0.0F;
    if (intent.fire && fire_cooldown == 0 && has_aim) {
        const svanes::Transform &transform =
            world.GetComponent<svanes::Transform>(entity);
        SpawnBullet(world, entity, {transform.x, transform.y}, intent.aim,
                    kBulletSpeed);
        fire_cooldown = kFireInterval;
    }

    if (flying) {
        state = GooseState::Flying;
    } else if (intent.move != 0.0F) {
        state = GooseState::Walking;
    } else {
        state = GooseState::Idle;
    }
}

// TASK 5, collision response: checks the goose against every Solid entity and
// responds to each overlap by pushing the goose back out along the collision
// normal, then killing the velocity that drove it into the surface. Landing on
// something is read off the same normals, so grounded is recalculated here rather
// than tracked separately
void Goose::ResolveCollisions(svanes::Registry &world) {
    grounded = false;

    svanes::Transform &transform = world.GetComponent<svanes::Transform>(entity);
    svanes::Kinematic2D &motion =
        world.GetComponent<svanes::Kinematic2D>(entity);
    const svanes::Geometry2D body =
        world.GetComponent<svanes::Collider2D>(entity).geometry;

    world.ForEach<svanes::Transform, svanes::Collider2D, Solid>(
        [&](svanes::Entity, const svanes::Transform &other_transform,
            const svanes::Collider2D &other_collider, const Solid &) {
            const std::vector<svanes::Collision2D> collisions =
                svanes::DetectCollisions(body, transform,
                                         other_collider.geometry,
                                         other_transform);

            for (const svanes::Collision2D &collision : collisions) {
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
        });
}

void Goose::Hit(svanes::Registry &world, svanes::Vector2D direction,
                float speed, std::uint32_t damage) {
    if (IsDead() || invincible_timer > 0) {
        return;
    }

    const float length = std::hypot(direction.x, direction.y);
    if (!std::isfinite(length) || length <= 0.0F) {
        throw std::invalid_argument(
            "Goose::Hit requires a finite and nonzero direction.");
    }

    const svanes::Vector2D velocity =
        direction / length * svanes::PerSecondToPerTic(speed);

    svanes::Kinematic2D &motion =
        world.GetComponent<svanes::Kinematic2D>(entity);
    motion.velocity_x = velocity.x;
    motion.velocity_y = velocity.y;

    knockback_timer = kKnockbackLockTics;
    invincible_timer = kInvincibleTics;
    dash_timer = 0;
    grounded = false;
    health -= std::min(health, damage);
    if (health == 0) {
        Die();
    }
}

void Goose::Kill() {
    if (!IsDead()) {
        Die();
    }
}

void Goose::Die() {
    health = 0;
    respawn_timer = kRespawnTics;
}

bool Goose::IsDead() const { return health == 0; }

std::uint32_t Goose::GetHealth() const { return health; }

void Goose::Respawn(svanes::Registry &world) {
    svanes::Transform &transform = world.GetComponent<svanes::Transform>(entity);
    transform.x = spawn.x;
    transform.y = spawn.y;
    world.GetComponent<svanes::Kinematic2D>(entity) = svanes::Kinematic2D{};

    health = GooseMaxHealth;
    state = GooseState::Idle;
    grounded = false;
    fly_time_remaining = kMaxFlyTics;
    dash_timer = 0;
    dash_cooldown = 0;
    fire_cooldown = 0;
    knockback_timer = 0;
    invincible_timer = 0;
    respawn_timer = 0;
}

svanes::Entity Goose::GetEntity() const { return entity; }

GooseState Goose::GetState() const { return state; }

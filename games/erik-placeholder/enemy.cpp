#include "enemy.hpp"

#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr svanes::Color kEnemyColor{200, 60, 60, 255};
constexpr svanes::Color kEnemyBulletColor{255, 90, 90, 255};

}

void Enemy::Spawn(svanes::Registry& world, svanes::Vector2D position, float size, float health)
{
    if (alive) {
        throw std::logic_error("Enemy::Spawn called on an enemy that is already alive.");
    }

    if (!std::isfinite(size) || size <= 0.0F) {
        throw std::invalid_argument("Enemy::Spawn requires a finite and positive size.");
    }

    if (!std::isfinite(health) || health <= 0.0F) {
        throw std::invalid_argument("Enemy::Spawn requires a finite and positive health.");
    }

    const svanes::Rectangle2D body{
        .width = size,
        .height = size,
    };

    if (!spawned) {
        entity = world.CreateEntity();
    }
    spawned = true;
    body_size = size;
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{
        .x = position.x,
        .y = position.y,
    });
    world.AddComponent<svanes::SolidShape>(entity, svanes::SolidShape{
        .color = kEnemyColor,
        .geometry = body,
    });
    world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
    world.AddComponent<Health>(entity, Health{
        .current = health,
        .max = health,
    });

    alive = true;
    fire_cooldown = 0;
}

void Enemy::Update(const svanes::FrameContext& frame, const EnemyIntent& intent)
{
    Advance(frame.world, intent, frame.real_delta_tics);
}

void Enemy::Advance(svanes::Registry& world, const EnemyIntent& intent, svanes::TicCount delta_tics)
{
    if (!alive || delta_tics == 0) {
        return;
    }

    svanes::Transform& transform = world.GetComponent<svanes::Transform>(entity);

    const svanes::Vector2D offset{intent.move_to.x - transform.x, intent.move_to.y - transform.y};
    const float distance = std::hypot(offset.x, offset.y);
    const float delta_seconds = static_cast<float>(delta_tics) /
        static_cast<float>(svanes::TicsPerSecond);
    const float step = move_speed * delta_seconds;

    if (distance > step && distance > 0.0F) {
        transform.x += offset.x / distance * step;
        transform.y += offset.y / distance * step;
    } else {
        transform.x = intent.move_to.x;
        transform.y = intent.move_to.y;
    }

    fire_cooldown -= std::min(fire_cooldown, delta_tics);

    if (!intent.fire || fire_cooldown > 0) {
        return;
    }

    const svanes::Vector2D origin{transform.x, transform.y};
    const svanes::Vector2D direction{intent.aim_point.x - origin.x, intent.aim_point.y - origin.y};

    if (direction.x == 0.0F && direction.y == 0.0F) {
        return;
    }

    SpawnBullet(world, entity, origin, direction, bullet_speed, kEnemyBulletColor);
    fire_cooldown = fire_interval;
}

EnemySnapshot Enemy::Capture(const svanes::Registry& world) const
{
    if (!spawned) {
        throw std::logic_error("Enemy::Capture requires a spawned enemy.");
    }
    return {world.GetComponent<svanes::Transform>(entity), world.GetComponent<Health>(entity), fire_cooldown, alive};
}

void Enemy::Restore(svanes::Registry& world, const EnemySnapshot& snapshot)
{
    if (!spawned) {
        throw std::logic_error("Enemy::Restore requires a spawned enemy.");
    }
    world.GetComponent<svanes::Transform>(entity) = snapshot.transform;
    world.GetComponent<Health>(entity) = snapshot.health;
    fire_cooldown = snapshot.fire_cooldown;
    alive = snapshot.alive;
    if (alive) {
        const svanes::Rectangle2D body{.width = body_size, .height = body_size};
        world.AddComponent<svanes::Collider2D>(entity, svanes::Collider2D{body});
        world.AddComponent<svanes::SolidShape>(entity, svanes::SolidShape{.color = kEnemyColor, .geometry = body});
    } else {
        world.RemoveComponent<svanes::Collider2D>(entity);
        world.RemoveComponent<svanes::SolidShape>(entity);
    }
}

void Enemy::ApplyDamage(svanes::Registry& world, float amount)
{
    if (!alive) {
        return;
    }

    if (!std::isfinite(amount) || amount < 0.0F) {
        throw std::invalid_argument("Enemy::ApplyDamage requires a finite and nonnegative amount.");
    }

    Health& health = world.GetComponent<Health>(entity);
    health.current -= amount;

    if (health.current <= 0.0F) {
        world.RemoveComponent<svanes::Collider2D>(entity);
        world.RemoveComponent<svanes::SolidShape>(entity);
        alive = false;
    }
}

svanes::Vector2D Enemy::Position(svanes::Registry& world) const
{
    if (!alive) {
        throw std::logic_error("Enemy::Position called on a dead enemy.");
    }

    const svanes::Transform& transform = world.GetComponent<svanes::Transform>(entity);
    return {transform.x, transform.y};
}

bool Enemy::IsAlive() const
{
    return alive;
}

svanes::Entity Enemy::GetEntity() const
{
    return entity;
}

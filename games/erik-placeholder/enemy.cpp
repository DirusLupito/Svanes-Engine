#include "enemy.hpp"

#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry.hpp>
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

    entity = world.CreateEntity();
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
}

void Enemy::Update(const svanes::FrameContext& frame, const EnemyIntent& intent)
{
    if (!alive) {
        return;
    }

    svanes::Transform& transform = frame.world.GetComponent<svanes::Transform>(entity);

    const svanes::Vector2D offset{intent.move_to.x - transform.x, intent.move_to.y - transform.y};
    const float distance = std::hypot(offset.x, offset.y);
    const float step = move_speed * frame.delta_seconds;

    if (distance > step && distance > 0.0F) {
        transform.x += offset.x / distance * step;
        transform.y += offset.y / distance * step;
    } else {
        transform.x = intent.move_to.x;
        transform.y = intent.move_to.y;
    }

    fire_cooldown = std::max(fire_cooldown - frame.delta_seconds, 0.0F);

    if (!intent.fire || fire_cooldown > 0.0F) {
        return;
    }

    const svanes::Vector2D origin{transform.x, transform.y};
    const svanes::Vector2D direction{intent.aim_point.x - origin.x, intent.aim_point.y - origin.y};

    if (direction.x == 0.0F && direction.y == 0.0F) {
        return;
    }

    SpawnBullet(frame.world, entity, origin, direction, bullet_speed, kEnemyBulletColor);
    fire_cooldown = fire_interval;
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
        world.DestroyEntity(entity);
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

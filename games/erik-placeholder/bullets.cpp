#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

constexpr float kBulletSize = 8.0F;

bool IsOutsideBounds(const svanes::Transform& transform, const svanes::Rectangle2D& bounds)
{
    const float half_width = bounds.width * 0.5F;
    const float half_height = bounds.height * 0.5F;

    return transform.x < bounds.x - half_width
        || transform.x > bounds.x + half_width
        || transform.y < bounds.y - half_height
        || transform.y > bounds.y + half_height;
}

}

void SpawnBullet(
    svanes::Registry& world, svanes::Entity owner,
    svanes::Vector2D origin, svanes::Vector2D direction,
    float speed, svanes::Color color
)
{
    const float length = std::hypot(direction.x, direction.y);
    if (!std::isfinite(length) || length <= 0.0F) {
        throw std::invalid_argument("SpawnBullet requires a finite and nonzero direction.");
    }

    if (!std::isfinite(speed) || speed <= 0.0F) {
        throw std::invalid_argument("SpawnBullet requires a finite and positive speed.");
    }

    const svanes::Vector2D velocity = direction / length *
        svanes::PerSecondToPerTic(speed);

    const svanes::Rectangle2D body{
        .width = kBulletSize,
        .height = kBulletSize,
    };

    const svanes::Entity bullet = world.CreateEntity();
    world.AddComponent<svanes::Transform>(bullet, svanes::Transform{
        .x = origin.x,
        .y = origin.y,
    });
    world.AddComponent<svanes::Timeline>(bullet);
    world.AddComponent<svanes::Kinematic2D>(bullet, svanes::Kinematic2D{
        .velocity_x = velocity.x,
        .velocity_y = velocity.y,
    });
    world.AddComponent<svanes::SolidShape>(bullet, svanes::SolidShape{
        .color = color,
        .geometry = body,
    });
    world.AddComponent<svanes::Collider2D>(bullet, svanes::Collider2D{body});
    world.AddComponent<Bullet>(bullet, Bullet{.owner = owner});
}

std::vector<svanes::Entity> OrderedBullets(const svanes::Registry& world)
{
    std::vector<svanes::Entity> entities;
    world.ForEach<Bullet>([&](svanes::Entity entity, const Bullet&) {
        entities.push_back(entity);
    });
    std::sort(entities.begin(), entities.end(), [&](auto a, auto b) {
        const auto left = world.GetComponent<Bullet>(a).id;
        const auto right = world.GetComponent<Bullet>(b).id;
        if (left == right) {
            return a < b;
        }
        if (left == 0 || right == 0) {
            return right == 0;
        }
        return left < right;
    });
    return entities;
}

std::vector<BulletSnapshot> CaptureBullets(const svanes::Registry& world)
{
    std::vector<BulletSnapshot> snapshots;
    for (const auto entity : OrderedBullets(world)) {
        snapshots.push_back({world.GetComponent<Bullet>(entity),
            world.GetComponent<svanes::Transform>(entity),
            world.GetComponent<svanes::Kinematic2D>(entity),
            world.GetComponent<svanes::Timeline>(entity),
            world.GetComponent<svanes::SolidShape>(entity),
            world.GetComponent<svanes::Collider2D>(entity)});
    }
    return snapshots;
}

void RestoreBullets(svanes::Registry& world, const std::vector<BulletSnapshot>& snapshots)
{
    for (const auto entity : OrderedBullets(world)) {
        world.DestroyEntity(entity);
    }
    for (const auto& snapshot : snapshots) {
        const auto entity = world.CreateEntity();
        world.AddComponent<Bullet>(entity, snapshot.bullet);
        world.AddComponent<svanes::Transform>(entity, snapshot.transform);
        world.AddComponent<svanes::Kinematic2D>(entity, snapshot.motion);
        world.AddComponent<svanes::Timeline>(entity, snapshot.timeline);
        world.AddComponent<svanes::SolidShape>(entity, snapshot.shape);
        world.AddComponent<svanes::Collider2D>(entity, snapshot.collider);
    }
}

std::vector<BulletHit> UpdateBullets(svanes::Registry& world, const svanes::Rectangle2D& bounds)
{
    std::vector<svanes::Entity> targets;
    world.ForEach<svanes::Transform, svanes::Collider2D>(
        [&](svanes::Entity entity, const svanes::Transform&, const svanes::Collider2D&) {
            if (!world.HasComponent<Bullet>(entity)) {
                targets.push_back(entity);
            }
        });
    std::sort(targets.begin(), targets.end());
    std::vector<BulletHit> hits;
    for (const auto entity : OrderedBullets(world)) {
        const auto& bullet = world.GetComponent<Bullet>(entity);
        const auto& transform = world.GetComponent<svanes::Transform>(entity);
        if (IsOutsideBounds(transform, bounds)) {
            world.DestroyEntity(entity);
            continue;
        }
        const auto& collider = world.GetComponent<svanes::Collider2D>(entity);
        const auto& motion = world.GetComponent<svanes::Kinematic2D>(entity);
        for (const auto other : targets) {
            if (other == bullet.owner) {
                continue;
            }
            const auto collisions = svanes::DetectCollisions(collider.geometry, transform,
                world.GetComponent<svanes::Collider2D>(other).geometry,
                world.GetComponent<svanes::Transform>(other));
            if (!collisions.empty()) {
                hits.push_back({other, bullet.owner, {motion.velocity_x, motion.velocity_y}});
                world.DestroyEntity(entity);
                break;
            }
        }
    }
    return hits;
}

#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/deterministic_math.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

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
    const float length = svanes::Length(direction.x, direction.y);
    if (!std::isfinite(length) || length <= 0.0F) {
        throw std::invalid_argument("SpawnBullet requires a finite and nonzero direction.");
    }

    if (!std::isfinite(speed) || speed <= 0.0F) {
        throw std::invalid_argument("SpawnBullet requires a finite and positive speed.");
    }

    const svanes::Vector2D velocity = direction / length *
        svanes::PerSecondToPerTic(speed);

    const svanes::Rectangle2D body{
        .width = BulletSize,
        .height = BulletSize,
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
    world.AddComponent<Bullet>(bullet, Bullet{
        .owner = owner,
        .owner_id = world.GetComponent<svanes::StableId>(owner).value,
    });
}

std::vector<svanes::Entity> OrderedBullets(const svanes::Registry& world)
{
    return svanes::EntitiesByStableId<Bullet>(world);
}

std::vector<BulletSnapshot> CaptureBullets(const svanes::Registry& world)
{
    std::vector<BulletSnapshot> snapshots;
    for (const auto entity : OrderedBullets(world)) {
        snapshots.push_back({world.GetComponent<svanes::StableId>(entity),
            world.GetComponent<Bullet>(entity),
            world.GetComponent<svanes::Transform>(entity),
            world.GetComponent<svanes::Kinematic2D>(entity),
            world.GetComponent<svanes::Timeline>(entity),
            world.GetComponent<svanes::SolidShape>(entity),
            world.GetComponent<svanes::Collider2D>(entity)});
    }
    return snapshots;
}

void RestoreBullets(svanes::Registry& world, const std::vector<BulletSnapshot>& snapshots,
                    svanes::Entity departed_owner)
{
    std::vector<svanes::Entity> live;
    world.ForEach<Bullet>([&](svanes::Entity entity, const Bullet&) {
        live.push_back(entity);
    });
    for (const auto entity : live) {
        world.DestroyEntity(entity);
    }
    for (const auto& snapshot : snapshots) {
        Bullet bullet = snapshot.bullet;
        bullet.owner = svanes::FindByStableId(world, bullet.owner_id).value_or(departed_owner);
        const auto entity = world.CreateEntity();
        world.AddComponent<svanes::StableId>(entity, snapshot.id);
        world.AddComponent<Bullet>(entity, bullet);
        world.AddComponent<svanes::Transform>(entity, snapshot.transform);
        world.AddComponent<svanes::Kinematic2D>(entity, snapshot.motion);
        world.AddComponent<svanes::Timeline>(entity, snapshot.timeline);
        world.AddComponent<svanes::SolidShape>(entity, snapshot.shape);
        world.AddComponent<svanes::Collider2D>(entity, snapshot.collider);
    }
}

std::vector<BulletHit> UpdateBullets(svanes::Registry& world, const svanes::Rectangle2D& bounds,
                                     std::span<const svanes::Entity> targets)
{
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

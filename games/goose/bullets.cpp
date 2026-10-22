#include "bullets.hpp"

#include "minions.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <cmath>
#include <optional>
#include <stdexcept>

void SpawnBullet(svanes::Registry &world, svanes::Entity owner,
                 svanes::Vector2D origin, svanes::Vector2D direction,
                 float speed) {
    const float length = std::hypot(direction.x, direction.y);
    if (!std::isfinite(length) || length <= 0.0F) {
        throw std::invalid_argument(
            "SpawnBullet requires a finite and nonzero direction.");
    }

    const svanes::Vector2D velocity =
        direction / length * svanes::PerSecondToPerTic(speed);
    const std::optional<svanes::Entity> clock =
        world.GetComponent<svanes::Timeline>(owner).GetParent();

    const svanes::Entity bullet = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        bullet, svanes::Transform{.x = origin.x, .y = origin.y});
    world.AddComponent<svanes::Timeline>(bullet, svanes::Timeline{clock});
    world.AddComponent<svanes::Kinematic2D>(
        bullet,
        svanes::Kinematic2D{.velocity_x = velocity.x, .velocity_y = velocity.y});
    world.AddComponent<svanes::Collider2D>(
        bullet, svanes::Collider2D{
                    svanes::Rectangle2D{.width = BulletSize, .height = BulletSize}});
    world.AddComponent<Bullet>(bullet, Bullet{owner});
}

std::vector<BulletHit> UpdateBullets(svanes::Registry &world) {
    std::vector<svanes::Entity> targets;
    world.ForEach<svanes::Transform, svanes::Collider2D>(
        [&](svanes::Entity entity, const svanes::Transform &,
            const svanes::Collider2D &) {
            if (!world.HasComponent<Bullet>(entity)) {
                targets.push_back(entity);
            }
        });

    std::vector<BulletHit> hits;
    std::vector<svanes::Entity> struck;
    world.ForEach<Bullet, svanes::Transform, svanes::Collider2D,
                  svanes::Kinematic2D>(
        [&](svanes::Entity entity, const Bullet &bullet,
            const svanes::Transform &transform,
            const svanes::Collider2D &collider,
            const svanes::Kinematic2D &motion) {
            for (const svanes::Entity target : targets) {
                if (target == bullet.owner) {
                    continue;
                }
                if (world.HasComponent<Minion>(target) &&
                    world.GetComponent<Minion>(target).spawner ==
                        bullet.owner) {
                    continue;
                }

                const auto collisions = svanes::DetectCollisions(
                    collider.geometry, transform,
                    world.GetComponent<svanes::Collider2D>(target).geometry,
                    world.GetComponent<svanes::Transform>(target));
                if (!collisions.empty()) {
                    hits.push_back({target,
                                    bullet.owner,
                                    {motion.velocity_x, motion.velocity_y}});
                    struck.push_back(entity);
                    return;
                }
            }
        });

    for (const svanes::Entity entity : struck) {
        world.DestroyEntity(entity);
    }
    return hits;
}

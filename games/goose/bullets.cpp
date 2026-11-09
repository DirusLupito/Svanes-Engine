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

bool PassesThrough(const svanes::Registry &world, const Bullet &bullet,
                   svanes::Entity other) {
    if (other == bullet.owner || world.HasComponent<Bullet>(other)) {
        return true;
    }
    return world.HasComponent<Minion>(other) &&
           world.GetComponent<Minion>(other).spawner == bullet.owner;
}

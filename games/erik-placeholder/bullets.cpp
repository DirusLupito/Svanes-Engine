#include "bullets.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

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

    const svanes::Vector2D velocity = direction / length * speed;

    const svanes::Rectangle2D body{
        .width = kBulletSize,
        .height = kBulletSize,
    };

    const svanes::Entity bullet = world.CreateEntity();
    world.AddComponent<svanes::Transform>(bullet, svanes::Transform{
        .x = origin.x,
        .y = origin.y,
    });
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

void UpdateBullets(svanes::Registry& world, const svanes::Rectangle2D& bounds)
{
    std::vector<svanes::Entity> destroyed;

    world.ForEach<Bullet, svanes::Transform, svanes::Collider2D>(
        [&](svanes::Entity entity, Bullet& bullet, svanes::Transform& transform, svanes::Collider2D& collider) {
            if (IsOutsideBounds(transform, bounds)) {
                destroyed.push_back(entity);
                return;
            }

            bool hit = false;

            world.ForEach<svanes::Transform, svanes::Collider2D>(
                [&](svanes::Entity other, svanes::Transform& other_transform, svanes::Collider2D& other_collider) {
                    if (hit || other == entity || other == bullet.owner) {
                        return;
                    }

                    if (world.HasComponent<Bullet>(other)) {
                        return;
                    }

                    const std::vector<svanes::Collision2D> collisions = svanes::DetectCollisions(
                        collider.geometry, transform, other_collider.geometry, other_transform
                    );

                    if (!collisions.empty()) {
                        hit = true;
                    }
                }
            );

            if (hit) {
                destroyed.push_back(entity);
            }
        }
    );

    for (const svanes::Entity entity : destroyed) {
        world.DestroyEntity(entity);
    }
}

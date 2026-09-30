#include "damage_system.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

bool ApplyDamage(svanes::Registry &world, svanes::Entity target, float amount) {
    if (!std::isfinite(amount) || amount < 0.0F) {
        throw std::invalid_argument("Damage must be finite and nonnegative.");
    }
    if (!world.HasComponent<Health>(target) ||
        !world.HasComponent<svanes::Collider2D>(target)) {
        return false;
    }

    auto &health = world.GetComponent<Health>(target);
    health.remaining = std::max(0.0F, health.remaining - amount);
    return health.remaining == 0.0F;
}

bool IsDead(const svanes::Registry &world, svanes::Entity entity) {
    return world.HasComponent<Health>(entity) &&
           world.GetComponent<Health>(entity).remaining <= 0.0F;
}

/**
 * Finds the distance from a point to the center of a transformed primitive.
 *
 * @tparam Shape The type of the primitive shape (e.g., Circle2D, Rectangle2D).
 *
 * @param shape The primitive in its own local coordinate system.
 * @param pose The transform placing it in the world.
 * @param point The world-space point to measure from.
 *
 * @return The distance to the primitive's center.
 */
template <typename Shape>
static float DistanceToCenter(const Shape &shape, const svanes::Transform &pose,
                              svanes::Vector2D point) {
    const auto center = svanes::ComputeCenter(shape);
    const auto transformed =
        svanes::ComposeTransforms(pose, {center.x, center.y});

    return std::hypot(transformed.x - point.x, transformed.y - point.y);
}

/**
 * Finds the nearest part center in a composite. We only use one distance,
 * so overlapping parts do not make an entity take damage several times.
 *
 * @param shape The composite whose parts we want to examine.
 * @param pose The transform placing it in the world.
 * @param point The world-space point to measure from.
 *
 * @return The nearest distance, or infinity if there are no parts.
 */
static float DistanceToCenter(const svanes::CompositeShape2D &shape,
                              const svanes::Transform &pose,
                              svanes::Vector2D point) {
    float distance = std::numeric_limits<float>::infinity();

    for (const auto &part : shape.parts) {
        const auto transformed =
            svanes::ComposeTransforms(pose, part.transform);
        const float candidate =
            DistanceToCenter(part.shape, transformed, point);
        distance = std::min(distance, candidate);
    }

    return distance;
}

void ApplyAreaDamage(svanes::Registry &world, svanes::Vector2D center,
                     float radius, float damage) {
    if (!std::isfinite(radius) || radius <= 0.0F || !std::isfinite(damage) ||
        damage < 0.0F || !std::isfinite(center.x) || !std::isfinite(center.y)) {
        throw std::invalid_argument(
            "Blast center must be finite, radius finite and positive, and "
            "damage finite and nonnegative");
    }

    // for every entity with health, a transform, and a collider, find the
    // distance to the blast center and apply damage depending on the distance,
    // with full damage at the center and linearly decreasing to zero at the
    // radius.
    world.ForEach<Health, svanes::Transform, svanes::Collider2D>(
        [&](svanes::Entity entity, const Health &health,
            const svanes::Transform &pose, const svanes::Collider2D &collider) {
            if (health.remaining <= 0.0F) {
                return;
            }

            const float distance = std::visit(
                [&](const auto &shape) {
                    return DistanceToCenter(shape, pose, center);
                },
                collider.geometry);

            if (distance < radius) {
                ApplyDamage(world, entity, damage * (1.0F - distance / radius));
            }
        });
}

#pragma once

#include <svanes/entity.hpp>
#include <svanes/vector2d.hpp>

namespace svanes {
class Registry;
}

/**
 * Stores the remaining health of an entity that can receive damage.
 *
 * FIELDS:
 * - remaining: The entity's current health value. Defaults to 100.0.
 */
struct Health {
    float remaining = 100.0F;
};

/**
 * Applies damage to the target entity's Health component.
 *
 * Returns false if the target does not have Health and Collider2D components.
 *
 * @param world The registry containing the target entity and its components.
 * @param target The entity to which the damage is applied.
 * @param amount The amount of damage to apply.
 *
 * @return true if the target's remaining health is 0.0 after applying damage;
 *         otherwise, false.
 *
 * @throws std::invalid_argument if amount is not finite or is negative.
 */
bool ApplyDamage(svanes::Registry &world, svanes::Entity target, float amount);

/**
 * Checks whether an entity has a Health component whose remaining health is
 * at or below 0.0. Entities without Health are not considered dead.
 *
 * @param world The registry containing the entity.
 * @param entity The entity to check.
 * @return Whether the entity is dead. Entities without health are not dead.
 */
bool IsDead(const svanes::Registry &world, svanes::Entity entity);

/**
 * Applies damage around a point, falling linearly from full damage at the
 * center to no damage at the radius. Each entity is damaged once per blast.
 * For composites, we use the nearest part's center, not the composite's origin.
 * Only entities with Health, Transform, and Collider2D components are
 * considered.
 *
 * @param world The registry containing the targets.
 * @param center The world-space center of the blast.
 * @param radius The positive radius of the blast.
 * @param damage The nonnegative damage at the center of the blast.
 */
void ApplyAreaDamage(svanes::Registry &world, svanes::Vector2D center,
                     float radius, float damage);

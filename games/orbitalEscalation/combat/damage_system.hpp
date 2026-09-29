#pragma once

#include <svanes/entity.hpp>

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
 * Returns false if the target does not have a Health component.
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

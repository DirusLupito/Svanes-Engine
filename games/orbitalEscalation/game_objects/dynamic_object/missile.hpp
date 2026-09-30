#pragma once

#include "dynamic_object.hpp"
#include <svanes/timeline_system.hpp>

/**
 * Defines the properties of a missile. For now, a missile simply moves with
 * its initial velocity and is affected by gravity, like any other dynamic
 * TODO: Add guidance and propulsion.
 *
 * FIELDS:
 * - object: The missile's name, collider, visuals, and attachment definitions.
 * - health: The missile's initial health. A missile explodes when this runs
 * out.
 * - damage: The damage dealt at the center of its explosion.
 * - blast_radius: The distance at which its explosion stops dealing damage.
 * - arming_tics: The missile's local tics after launch before it can collide
 * with its firing ship. Other objects can be hit immediately.
 */
struct MissileDefinition {
    DynamicObjectDefinition object;
    float health;
    float damage;
    float blast_radius;
    svanes::TicCount arming_tics = 0;
};

/**
 * Represents a missile in the game world.
 * TODO: Add guidance and propulsion to missiles, so they can track targets and
 * accelerate.
 */
class Missile final : public DynamicObject {
public:
    /**
     * Constructs a Missile entity in the provided registry.
     *
     * @param world The registry in which to create the missile entity and its
     * components.
     * @param gameplay_timeline The parent timeline entity to which the
     * missile's Timeline will be linked.
     * @param definition The missile's dynamic object properties. The catalog
     * creates its attachments after constructing the missile itself.
     */
    Missile(svanes::Registry &world, svanes::Entity gameplay_timeline,
            MissileDefinition definition);
};

/**
 * Validates the health and explosion properties of a missile definition.
 *
 * @param definition The missile definition to validate.
 * @throws std::invalid_argument If health or blast_radius is not finite and
 * positive, or damage is not finite and nonnegative.
 */
void ValidateMissile(const MissileDefinition &definition);

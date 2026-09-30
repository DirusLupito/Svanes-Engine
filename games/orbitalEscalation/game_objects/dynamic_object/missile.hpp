#pragma once

#include "dynamic_object.hpp"

/**
 * Defines the properties of a missile. For now, a missile simply moves with
 * its initial velocity and is affected by gravity, like any other dynamic
 * TODO: Add guidance and propulsion.
 *
 * FIELDS:
 * - object: The missile's name, collider, visuals, and attachment definitions.
 */
struct MissileDefinition {
    DynamicObjectDefinition object;
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

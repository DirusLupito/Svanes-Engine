#pragma once

#include "visuals.hpp"

#include <svanes/attractor_system.hpp>
#include <svanes/collision_system.hpp>

namespace svanes {
class Registry;
}

/**
 * Defines the properties of a planet. A planet has a gravitational field, a
 * collision layer, and a set of visuals that represent its appearance in the
 * game world.
 *
 * FIELDS:
 * - Attractor: A PointAttractor2D that defines the planet's gravitational
 * field.
 * - Collider: A Collider2D that defines the planet's collision layer.
 * - Visuals: A vector of Visual objects that define the planet's appearance in
 * the game world.
 */
struct PlanetDefinition {
    svanes::PointAttractor2D attractor;
    svanes::Collider2D collider;
    std::vector<Visual> visuals;
};

/**
 * Represents a planet in the game world, consisting of a rendered surface,
 * a collision layer, and a gravitational field.
 */
class Planet final {
public:
    /**
     * Constructs a Planet entity in the provided registry.
     *
     * @param world The registry in which to create the planet entity and its
     * components.
     * @param definition The planet's gravitational field, collider, and
     * visuals.
     */
    Planet(svanes::Registry &world, PlanetDefinition definition);

    /**
     * Returns the unique identifier of the planet entity.
     *
     * @return The unique identifier of the planet entity.
     */
    svanes::Entity GetEntity() const;

    /**
     * Returns a reference to the Transform component of the planet entity.
     *
     * @param world The registry containing the planet entity and its
     * components.
     * @return A reference to the Transform component of the planet entity.
     */
    svanes::Transform &GetTransform(svanes::Registry &world) const;

    /**
     * Updates the visuals of the planet entity to match its current transform.
     *
     * @param world The registry containing the planet entity and its
     * components.
     */
    void UpdateVisuals(svanes::Registry &world) const;

private:
    // The unique identifier of the planet entity in the registry.
    svanes::Entity entity;

    // The Visuals object that manages the visual representation of the planet
    // entity.
    Visuals visuals;
};

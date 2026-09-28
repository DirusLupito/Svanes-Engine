#pragma once

#include <svanes/entity.hpp>

#include <array>

namespace svanes {
class Registry;
}

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
     */
    explicit Planet(svanes::Registry &world);

    /**
     * Returns the unique identifier of the planet entity.
     *
     * @return The unique identifier of the planet entity.
     */
    svanes::Entity GetEntity() const;

    /**
     * Returns the radius of the planet, which is used for collision detection
     * and gravitational calculations.
     *
     * @return The radius of the planet.
     */
    float GetRadius() const;

private:
    std::array<svanes::Entity, 3> layers;
};

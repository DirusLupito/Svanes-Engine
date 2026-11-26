#pragma once

#include <svanes/entity.hpp>
#include <svanes/vector2d.hpp>

#include <optional>

namespace svanes {
class Registry;
}

namespace marching_orders {

/**
 * Represents a single unit on the map. The unit class provides base functionality,
 * specific unit types will add their own specialized functionality.
 */
class Unit {
public:
    /**
     *
     * Consructor for a unit. Handles creating the entity in the game world and assigning necessary components.
     * 
     * @param world The game world to create the unit in.
     * @param position The initial position that the unit will be created at.
     * @param speed The rate at which this entity will move.
     *
     * @throws std::invalid_argument if the initial position is invalid/out of bounds.
     */
    Unit(svanes::Registry &world, svanes::Vector2D position, float speed);

    /**
     *
     * Helper that updates a given unit's destination. Units with a destination will move towards it.
     * 
     * @param world The game world containing the entity to have its destination set.
     * @param position The position in the game world that is to be set as the destination.
     *
     * @throws std::invalid_argument if the provided destination is invalid/out of bounds.
     */
    void SetDestination(svanes::Registry &world, svanes::Vector2D position);

    /**
     *
     * Handler for updating a unit's state each physics step.
     * 
     * @param world The game world containing the entity being updated.
     */
    void PhysicsUpdate(svanes::Registry &world);

    /**
     * 
     * Returns the entity representing this unit.
     * 
     * @return The entity representing this unit. I don't know what to tell ya.
     */
    svanes::Entity GetEntity() const;

private:
    // The entity ID for this unit.
    svanes::Entity entity;

    // This unit's destination for movement.
    std::optional<svanes::Vector2D> destination;

    // The base speed at which a unit will move towards its destination.
    float speed;
};

}

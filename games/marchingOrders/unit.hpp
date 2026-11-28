#pragma once

#include <svanes/entity.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <optional>

namespace svanes {
class Registry;
}

namespace marching_orders {

/**
 * The terrain of the game level. This is required for calculations
 * of move speed being affected by elevation as well as preventing
 * traversal over non traversable terrain.
 */
class Terrain;

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
     * @param terrain The terrain map of the game level the unit is in.
     * @param position The initial position that the unit will be created at.
     * @param speed The rate at which this entity will move.
     * @param team The id of the team that this unit belongs to.
     *
     * @throws std::invalid_argument if the initial position is invalid/out of bounds.
     */
    Unit(svanes::Registry &world, const Terrain &terrain,
         svanes::Vector2D position, float speed, std::uint32_t team = 0);

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

    /**
     * Returns the ID of the team that this unit belongs to.
     * 
     * @return This unit's team ID
     */
    std::uint32_t GetTeam() const;

private:
    // Changes unit move speed based on terrain conditions
    void UpdateVelocity(svanes::Registry &world);
    
    // Terrain map for calculating movement speed
    const Terrain &terrain;
    // The last position used for comparing terrain elevation change.
    svanes::Vector2D previous_position;

    // The entity ID for this unit.
    svanes::Entity entity;

    // This unit's destination for movement.
    std::optional<svanes::Vector2D> destination;

    // The base speed at which a unit will move towards its destination.
    float speed;

    // The ID of the team that this unit belongs to
    std::uint32_t team;
};

}

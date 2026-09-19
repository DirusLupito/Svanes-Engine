#pragma once

#include <svanes/collision_system.hpp>
#include <svanes/entity.hpp>

#include <vector>

namespace svanes {

class Registry;

/**
 * Represents a collision between two entities in the world. Contains the
 * entities involved in the collision and the details of the collision.
 *
 * FIELDS:
 * - a: The first entity involved in the collision.
 * - b: The second entity involved in the collision.
 * - collisions: A vector of Collision2D objects representing the details of
 *   the collision, including penetration depth and normal.
 */
struct EntityCollision2D {
    Entity a;
    Entity b;
    std::vector<Collision2D> collisions;
};

/**
 * Given a list of entities, detects all collisions between them and returns a
 * vector of EntityCollision2D objects representing the collisions.
 *
 * @param world The registry containing the entities and their components.
 * @param entities A vector of entities to check for collisions.
 * @return A vector of EntityCollision2D objects representing the detected
 * collisions between the entities.
 */
std::vector<EntityCollision2D>
DetectEntityCollisions(const Registry &world,
                       const std::vector<Entity> &entities);

} // namespace svanes

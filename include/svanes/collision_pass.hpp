#pragma once

#include <svanes/collision_system.hpp>
#include <svanes/entity.hpp>

#include <cstddef>
#include <vector>

namespace svanes {

class AsyncParallelForDriver;
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
 * vector of EntityCollision2D objects representing the collisions. The world
 * must remain unchanged until this call returns.
 *
 * @param world The registry containing the entities and their components.
 * @param entities A vector of entities to check for collisions.
 * @param driver The driver used for both broad and narrow phase work.
 * @param batch_size The maximum number of entries or candidate pairs per
 * batch. Must be positive. Defaults to 16 for both phases.
 * @return A vector of EntityCollision2D objects representing the detected
 * collisions between the entities, in unspecified order. Each pair puts the
 * smaller entity ID first so games may choose to use this to sort the
 * collisions in a consistent order if desired.
 */
std::vector<EntityCollision2D> DetectEntityCollisions(
    const Registry &world, const std::vector<Entity> &entities,
    AsyncParallelForDriver &driver, std::size_t batch_size = 16);

} // namespace svanes

#pragma once

#include <svanes/collision_system.hpp>
#include <svanes/entity.hpp>

#include <cstddef>
#include <vector>

namespace svanes {

class AsyncParallelForDriver;
class EventManager;
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
 * Determines which object is first in a collision test.
 *
 * MEMBERS:
 * - EntityId: The smaller local entity ID is first. Event order is
 * unspecified.
 * - InputOrder: The entity appearing first in the supplied list is first.
 * Events are queued by the two entities' positions in that list. This lets a
 * game supply shared identity order even when local entity IDs differ.
 */
enum class CollisionOrder { EntityId, InputOrder };

/**
 * Given a list of entities, detects collisions between them and queues one
 * CollisionEvent for each detected pair. The world must remain unchanged
 * until this call returns. Handlers run when the event manager dispatches
 * the queued events.
 *
 * @param world The registry containing the entities and their components.
 * @param entities A vector of entities to check for collisions. Each must
 * have Collider2D and Transform components.
 * @param events The event manager to queue detected collisions for later
 * delivery.
 * @param driver The driver used for both broad and narrow phase work.
 * @param batch_size The maximum number of entries or candidate pairs per
 * batch. Must be positive. Defaults to 16 for both phases.
 * @param order The ordering used before narrow-phase testing and when queuing
 * collision events.
 *
 * @throws std::invalid_argument if batch_size is zero, entity IDs repeat, or
 * collision geometry is invalid.
 * @throws std::out_of_range if a required component is missing.
 */
void DetectEntityCollisions(const Registry &world,
                            const std::vector<Entity> &entities,
                            EventManager &events,
                            AsyncParallelForDriver &driver,
                            std::size_t batch_size = 16,
                            CollisionOrder order = CollisionOrder::EntityId);

} // namespace svanes

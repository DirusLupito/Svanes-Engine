#pragma once

#include "owned_entity.hpp"

#include <svanes/vector2d.hpp>

#include <functional>
#include <vector>

namespace svanes {
class CollisionEvent;
class EventManager;
} // namespace svanes

/**
 * Marks a world boundary that removes colliding objects without physical
 * collision response.
 */
struct DeletionBox {};

/**
 * Creates the invisible collision boxes that delete gameplay objects
 * at the world's boundaries.
 *
 * @param world The registry in which to create the boxes' colliders.
 * @return The owners of the boxes' collision entities.
 */
std::vector<OwnedEntity> CreateDeletionBoxes(svanes::Registry &world);

/**
 * Marks objects touching deletion boxes as dead and reports their positions
 * for immediate visual effects.
 */
class DeletionBoxCollisionHandler final {
public:
    /**
     * Registers the deletion-box handler to receive collision events.
     *
     * @param world The registry containing the objects and deletion boxes.
     * @param events The event manager receiving collisions for this world.
     * @param on_delete An optional callback invoked when an object is marked dead
     * by a deletion box. Receives the object's world position.
     *
     * @throws std::invalid_argument if this handler type is already registered.
     * @throws std::logic_error if called during an event callback.
     */
    void Register(svanes::Registry &world, svanes::EventManager &events,
                  std::function<void(svanes::Vector2D)> on_delete = {});

private:
    /**
     * Marks the object touching a deletion box as dead, including objects
     * without health. Already dead objects and contacts between deletion boxes
     * produce no deletion effect.
     *
     * @param world The registry containing the colliding entities.
     * @param event The collision to check for a deletion-box contact.
     * @param on_delete Optional callback invoked when an object is marked dead by a deletion box.
     * Receives the object's world position.
     */
    void
    HandleCollision(svanes::Registry &world,
                    const svanes::CollisionEvent &event,
                    const std::function<void(svanes::Vector2D)> &on_delete);
};

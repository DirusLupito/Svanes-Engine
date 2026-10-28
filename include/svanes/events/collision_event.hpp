#pragma once

#include <svanes/collision_pass.hpp>
#include <svanes/events/event.hpp>

#include <string>

namespace svanes {

/**
 * Reports a detected collision between two entities.
 * Owns the collision record so it can be
 * delivered after the detection results have been released.
 */
class CollisionEvent final : public Event {
public:
    /**
     * Constructs an event describing a collision between two entities.
     *
     * @param collision The colliding entities and their contact details.
     */
    explicit CollisionEvent(EntityCollision2D collision);

    /**
     * Returns the identifier used to subscribe to collision events.
     *
     * @return The event type identifier shared by all CollisionEvent instances.
     */
    static std::string GetStaticEventType();

    /**
     * Identifies this event so the event manager can find its handlers.
     *
     * @return The same identifier returned by GetStaticEventType().
     */
    const std::string GetEventType() const override;

    // The colliding entities and their contact details.
    EntityCollision2D collision;
};

} // namespace svanes

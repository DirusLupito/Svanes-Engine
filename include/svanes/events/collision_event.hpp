#pragma once

#include <svanes/collision_pass.hpp>
#include <svanes/events/event.hpp>

#include <string_view>

namespace svanes {

/**
 * Reports a detected collision between two entities.
 * Owns the collision record so it can be
 * delivered after the detection results have been released.
 */
class CollisionEvent final : public EventOf<CollisionEvent> {
public:
    // The unique name of this event type, hashed to produce its EventTypeId.
    static constexpr std::string_view EventName = "svanes.collision";

    /**
     * Constructs an event describing a collision between two entities.
     *
     * @param collision The colliding entities and their contact details.
     */
    explicit CollisionEvent(EntityCollision2D collision);

    // The colliding entities and their contact details.
    EntityCollision2D collision;
};

} // namespace svanes

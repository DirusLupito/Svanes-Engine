#include <svanes/events/collision_event.hpp>

#include <utility>

namespace svanes {

CollisionEvent::CollisionEvent(EntityCollision2D collision)
    : collision(std::move(collision)) {}

std::string CollisionEvent::GetStaticEventType() { return "svanes.collision"; }

const std::string CollisionEvent::GetEventType() const {
    return GetStaticEventType();
}

} // namespace svanes

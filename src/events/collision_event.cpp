#include <svanes/events/collision_event.hpp>

#include <utility>

namespace svanes {

CollisionEvent::CollisionEvent(EntityCollision2D collision)
    : collision(std::move(collision)) {}

} // namespace svanes

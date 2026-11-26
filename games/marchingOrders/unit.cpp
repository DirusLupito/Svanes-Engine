#include "unit.hpp"

#include <svanes/geometry/geometry.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <cmath>
#include <stdexcept>

namespace marching_orders {

Unit::Unit(svanes::Registry &world, svanes::Vector2D position, float speed)
    : speed(svanes::PerSecondToPerTic(speed)) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(speed) || speed <= 0.0F) {
        throw std::invalid_argument(
            "Unit requires a finite position and positive speed.");
    }
    entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::Timeline>(entity);
    world.AddComponent<svanes::Kinematic2D>(entity);
}

void Unit::SetDestination(svanes::Registry &world, svanes::Vector2D position) {
    auto &transform = world.GetComponent<svanes::Transform>(entity);
    const auto offset = position - svanes::Vector2D{transform.x, transform.y};
    const float distance = std::hypot(offset.x, offset.y);
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(distance)) {
        throw std::invalid_argument(
            "Unit destination must be finite and reachable.");
    }
    auto &motion = world.GetComponent<svanes::Kinematic2D>(entity);
    if (distance == 0.0F) {
        motion.velocity_x = 0.0F;
        motion.velocity_y = 0.0F;
        destination.reset();
        return;
    }
    destination = position;
    transform.rotation = std::atan2(offset.y, offset.x);
    motion.velocity_x = (offset.x / distance) * speed;
    motion.velocity_y = (offset.y / distance) * speed;
}

void Unit::PhysicsUpdate(svanes::Registry &world) {
    if (!destination) {
        return;
    }
    auto &transform = world.GetComponent<svanes::Transform>(entity);
    auto &motion = world.GetComponent<svanes::Kinematic2D>(entity);
    const auto offset =
        *destination - svanes::Vector2D{transform.x, transform.y};
    if (offset.x * motion.velocity_x + offset.y * motion.velocity_y <= 0.0F) {
        transform.x = destination->x;
        transform.y = destination->y;
        motion.velocity_x = 0.0F;
        motion.velocity_y = 0.0F;
        destination.reset();
    }
}

svanes::Entity Unit::GetEntity() const { return entity; }

}

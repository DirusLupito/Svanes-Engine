#pragma once

#include <svanes/collision_system.hpp>
#include <svanes/events/collision_event.hpp>
#include <svanes/kinematic_system.hpp>

#include <optional>
#include <vector>

namespace svanes {
class EventManager;
class Registry;
class AsyncParallelForDriver;
}

struct DoubleTimeSolid {};

struct DoubleTimeTrigger {
    enum class Kind { Hazard, Collectible, Goal };
    Kind kind;
};

class DoubleTimeCollisionHandler final {
public:
    void Register(svanes::Registry &world, svanes::EventManager &events);
    void BeginStep();
    void Resolve(svanes::Registry &world, svanes::EventManager &events,
                 svanes::AsyncParallelForDriver &driver,
                 svanes::Entity character, svanes::Entity tilemap);
    void DetectTriggers(svanes::Registry &world, svanes::EventManager &events,
                        svanes::AsyncParallelForDriver &driver,
                        svanes::Entity probe);
    bool IsGrounded() const;
    std::optional<svanes::Entity> GetSupport() const;
    const std::vector<svanes::Entity> &GetTriggers() const;

private:
    void HandleCollision(svanes::Registry &world,
                         const svanes::CollisionEvent &event);
    std::optional<svanes::Entity> character;
    std::optional<svanes::Entity> support;
    std::optional<svanes::Collision2D> deepest;
    svanes::Transform origin;
    bool horizontal = true;
    bool detecting_triggers = false;
    bool grounded = false;
    std::vector<svanes::Entity> triggers;
};

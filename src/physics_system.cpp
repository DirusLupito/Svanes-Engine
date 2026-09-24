#include <svanes/physics_system.hpp>

#include <svanes/geometry/geometry.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>

#include <stdexcept>
#include <vector>

namespace svanes {

void AdvancePhysics(
    Registry &world, Vector2D gravity, AsyncParallelForDriver &driver,
    const std::function<void(std::span<const PhysicsTimeStep>)> &after_step) {
    if (!after_step) {
        throw std::invalid_argument("Physics requires an after-step callback.");
    }

    std::vector<PhysicsTimeStep> steps;
    // Physics are relevant to entities with Transform (position and rotation),
    // Kinematic2D (motion), and Timeline (local time).
    world.ForEach<Transform, Kinematic2D, Timeline>(
        [&](Entity entity, const Transform &, const Kinematic2D &,
            const Timeline &timeline) {
            steps.push_back({entity, timeline.GetDeltaTics()});
        });

    // very basic physics system at the moment:
    // just update the kinematics of all entities
    // and then call the game specific post-physics update callback.

    AdvanceKinematics(world, gravity, driver, steps);
    after_step(steps);
}

} // namespace svanes

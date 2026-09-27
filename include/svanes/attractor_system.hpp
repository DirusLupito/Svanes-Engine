#pragma once

#include <svanes/entity.hpp>
#include <svanes/physics_system.hpp>
#include <svanes/vector2d.hpp>

#include <functional>
#include <optional>
#include <vector>

namespace svanes {

class Registry;
class AsyncParallelForDriver;

/**
 * Represents a point attractor in a 2D space,
 * which can influence the acceleration of other entities based on their
 * relative position to the attractor. Alternatively, it can be a repulsor if
 * the acceleration is in the opposite direction of the offset.
 *
 * FIELDS:
 *
 * =======
 *
 * - accelerationField: The acceleration field function. Implemented by the user
 * to define how the attractor influences other entities based on their relative
 * position to the attractor.
 *
 *          @param offset_to_source: The offset from the target entity to the attractor.
 *                                   Measured as (attractor_position - target_position).
 *
 *          @return Vector2D: Acceleration in world units per target local tic
 *          squared.
 *
 * =======
 *
 * - cutoff_radius: An optional cutoff radius. If provided, the attractor will
 * only influence entities within this radius. If the distance from the
 * attractor to the target entity is at least this radius, the attractor will
 * have no effect on that entity.
 *
 * - allow_parallel: Whether accelerationField may be called concurrently for
 * different entities, including alongside other parallel attractors. Defaults
 * to false. Serial attractors run on the calling thread after all parallel
 * attractors finish. The attraction field callback must not modify the registry
 * or physics inputs during evaluation, and any captured data must remain valid.
 * It is the individual game programmer's responsibility to ensure that parallel
 * callbacks are parallel safe, and synchronize access to any mutable shared
 * state.
 */
struct PointAttractor2D {
    std::function<Vector2D(Vector2D offset_to_source)> accelerationField;
    std::optional<float> cutoff_radius;
    bool allow_parallel = false;
};

/**
 * Evaluates all PointAttractor2D components in the given world and computes the
 * resulting accelerations for all entities that are influenced by these
 * attractors. Parallel attractors are evaluated first in one parallel-for,
 * followed by serial attractors on the calling thread. Call after
 * AdvanceTimelines and before moving entities.
 *
 * @param world The registry containing all entities and their components.
 * @param entity_steps The entities participating in the current physics step,
 * along with their local tic deltas. These are the entities with Transform,
 * Kinematic2D, and Timeline components collected by the physics pass. Entries
 * with zero delta_tics are ignored. Each entity appears once.
 * @param driver The driver used to evaluate parallel attractors.
 * @param batch_size The maximum number of target entries per parallel work
 * batch. Must be positive. Defaults to 64.
 *
 * @return Acceleration vectors in entity_steps order. Entries with zero
 * delta_tics or no affecting attractors receive zero acceleration.
 */
std::vector<Vector2D>
EvaluateAttractors(const Registry &world,
                   std::span<const PhysicsTimeStep> entity_steps,
                   AsyncParallelForDriver &driver, std::size_t batch_size = 64);

} // namespace svanes

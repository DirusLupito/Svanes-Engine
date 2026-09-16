#include <svanes/kinematic_system.hpp>

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/attractor_system.hpp>
#include <svanes/geometry.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace svanes {

/**
 * Validates that the provided limit is either std::nullopt or a finite,
 * nonnegative value.
 *
 * @param limit The optional limit to validate.
 *
 * @throws std::invalid_argument if the limit is not std::nullopt and is either
 * not finite or negative.
 */
static void ValidateLimit(std::optional<float> limit) {
    if (limit && (!std::isfinite(*limit) || *limit < 0.0F)) {
        throw std::invalid_argument(
            "Kinematic2D magnitude limits must be finite and nonnegative.");
    }
}

/**
 * Clamps the magnitude of a 2D vector (x, y) to the specified limit.
 * If the limit is std::nullopt, no clamping is performed.
 *
 * @param x The x component of the vector, passed by reference.
 * @param y The y component of the vector, passed by reference.
 * @param limit The optional limit on the magnitude of the vector.
 *
 * @throws std::invalid_argument if the limit is not std::nullopt and is either
 * not finite or negative.
 */
static void ClampMagnitude(float &x, float &y, std::optional<float> limit) {
    ValidateLimit(limit);
    if (limit) {
        const float magnitude = std::hypot(x, y);
        if (magnitude > *limit) {
            const float scale = *limit / magnitude;
            x *= scale;
            y *= scale;
        }
    }
}

/**
 * Clamps the absolute value of a scalar to the specified limit.
 * If the limit is std::nullopt, no clamping is performed.
 *
 * @param value The scalar value to clamp, passed by reference.
 * @param limit The optional limit on the absolute value of the scalar.
 *
 * @throws std::invalid_argument if the limit is not std::nullopt and is either
 * not finite or negative.
 */
static void ClampMagnitude(float &value, std::optional<float> limit) {
    ValidateLimit(limit);
    if (limit) {
        value = std::clamp(value, -*limit, *limit);
    }
}

/**
 * Parallelizable function that advances the kinematic state of a single entity.
 * This function is designed to be called in parallel by the
 * AsyncParallelForDriver.
 *
 * @param transform The Transform component of the entity, representing its
 * position and rotation.
 * @param motion The Kinematic2D component of the entity, representing its
 * motion state.
 * @param acceleration The total acceleration to be applied to the entity, which
 * includes contributions from attractors and global acceleration fields.
 * @param delta_tics The elapsed local tics to advance the kinematic
 * state.
 */
static void AdvanceKinematic(Transform &transform, Kinematic2D &motion,
                             Vector2D acceleration, float delta_tics) {
    //
    // Clamp accelerations
    //

    ClampMagnitude(acceleration.x, acceleration.y, motion.max_acceleration);
    ClampMagnitude(motion.angular_acceleration,
                   motion.max_angular_acceleration);

    //
    // Velocity update from acceleration
    //

    motion.velocity_x += acceleration.x * delta_tics;
    motion.velocity_y += acceleration.y * delta_tics;
    motion.angular_velocity += motion.angular_acceleration * delta_tics;

    //
    // Velocity clamping
    //

    ClampMagnitude(motion.velocity_x, motion.velocity_y, motion.max_speed);
    ClampMagnitude(motion.angular_velocity, motion.max_angular_speed);

    //
    // Position and rotation update from velocity
    //

    transform.x += motion.velocity_x * delta_tics;
    transform.y += motion.velocity_y * delta_tics;
    transform.rotation += motion.angular_velocity * delta_tics;
}

void AdvanceKinematics(Registry &world, Vector2D gravity,
                       AsyncParallelForDriver &driver,
                       std::span<const PhysicsTimeStep> entity_steps) {
    if (!std::isfinite(gravity.x) || !std::isfinite(gravity.y)) {
        throw std::invalid_argument("Kinematics gravity must be finite.");
    }

    // Map of entity to the total acceleration applied to that entity by all
    // attractors.
    const auto attractions = EvaluateAttractors(world, entity_steps);

    std::vector<WorkItem> items;

    // We need only update those entities we already know are participating in
    // this physics step, rather than iterating over all entities with a
    // transform, kinematic, and timeline.
    for (const PhysicsTimeStep &step : entity_steps) {
        if (step.delta_tics == 0) {
            continue;
        }

        const Entity entity = step.entity;
        Transform &transform = world.GetComponent<Transform>(entity);
        Kinematic2D &motion = world.GetComponent<Kinematic2D>(entity);

        float acceleration_x = motion.acceleration_x;
        float acceleration_y = motion.acceleration_y;

        //
        // Contributions from point source attractors
        //

        const auto attraction = attractions.find(entity);
        if (attraction != attractions.end()) {
            acceleration_x += attraction->second.x;
            acceleration_y += attraction->second.y;
        }

        //
        // Contributions from global acceleration fields
        //

        if (world.HasComponent<Gravity>(entity)) {
            acceleration_x += gravity.x;
            acceleration_y += gravity.y;
        }

        items.push_back({&transform,
                         &motion,
                         {acceleration_x, acceleration_y},
                         step.delta_tics});
    }

    // Current hardcoded batch size. This will control the size of work an
    // individual thread will do before it checks for more work to do. Higher
    // values will reduce the overhead of thread management, but may lead to
    // less balanced work distribution if the work items vary significantly in
    // complexity. Lower values will increase the overhead of thread management,
    // but may lead to better load balancing. If all items of work are roughly
    // equal in complexity, AND all threads are scheduled to run on cores which
    // are roughly equal in performance, then the optimal batch size is the
    // number of items divided by the number of threads.
    //
    // Here, we hope this is the case.
    size_t batch_size =
        std::max<size_t>(1, items.size() / (driver.GetConcurrency()));

    // We wrap our actual work function in a lambda which forwards the work to
    // the AdvanceKinematic function to allow us to match the signature 
    // expected by the AsyncParallelForDriver's ParallelFor method.
    driver.ParallelFor(items.size(), batch_size,
                       [&](std::size_t begin, std::size_t end, std::uint32_t) {
                           for (std::size_t i = begin; i < end; ++i) {
                               const WorkItem &item = items[i];
                               AdvanceKinematic(
                                   *item.transform, *item.motion,
                                   item.acceleration,
                                   static_cast<float>(item.delta_tics));
                           }
                       });
}

} // namespace svanes

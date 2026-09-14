#pragma once

#include <svanes/vector2d.hpp>

#include <optional>

namespace svanes {

class Registry;
class AsyncParallelForDriver;

/**
 * Motion integrated into an entity's Transform. Acceleration persists until
 * changed. Limits must be finite and nonnegative; std::nullopt means unlimited.
 *
 * FIELDS:
 * - velocity_x: Horizontal velocity in world units per second.
 * - velocity_y: Vertical velocity in world units per second.
 *
 * ====
 *
 * - acceleration_x: Horizontal acceleration in world units per second squared.
 * - acceleration_y: Vertical acceleration in world units per second squared.
 *
 * ====
 *
 * - angular_velocity: Angular velocity in radians per second.
 * - angular_acceleration: Angular acceleration in radians per second squared.
 *
 * ====
 *
 * - max_speed: Optional limit on the 2-norm magnitude of linear velocity.
 * - max_acceleration: Optional limit on the 2-norm magnitude of linear
 * acceleration.
 * - max_angular_speed: Optional limit on the absolute angular velocity.
 * - max_angular_acceleration: Optional limit on the absolute angular
 * acceleration.
 */
struct Kinematic2D {
    float velocity_x = 0.0F;
    float velocity_y = 0.0F;

    float acceleration_x = 0.0F;
    float acceleration_y = 0.0F;

    float angular_velocity = 0.0F;
    float angular_acceleration = 0.0F;

    std::optional<float> max_speed;
    std::optional<float> max_acceleration;
    std::optional<float> max_angular_speed;
    std::optional<float> max_angular_acceleration;
};

/**
 * Represents a single work item for advancing the kinematic state of an entity.
 * Each work item contains pointers to the entity's Transform and Kinematic2D
 * components, as well as the total acceleration to be applied to the entity.
 *
 * FIELDS:
 * - transform: Pointer to the entity's Transform component.
 * - motion: Pointer to the entity's Kinematic2D component.
 * - acceleration: The total acceleration to be applied to the entity, which
 * includes contributions from attractors and global acceleration fields.
 */
struct WorkItem {
    Transform *transform;
    Kinematic2D *motion;
    Vector2D acceleration;
};

/**
 * Represents a gravity force applied to entities that have both the Kinematic2D
 * and Gravity components. Default is set to {0, 0}.
 *
 * Since we're in screen and world space, positive y is downwards. The gravity
 * vector is applied to the acceleration of entities with Kinematic2D and
 * Gravity components.
 */
struct Gravity {};


/**
 * Advances the kinematic state of all entities in the provided registry by the
 * specified time delta.
 *
 * @param world The registry containing all entities and their components.
 * @param delta_seconds The time delta in seconds to advance the kinematic
 * state.
 * @param gravity The gravity vector to apply to entities with Kinematic2D and
 * Gravity components.
 * @param driver The AsyncParallelForDriver to use for parallel execution of
 * the kinematic updates. If the driver has a concurrency of 1, the updates will
 * be executed sequentially on the calling thread.
 *
 * @throws std::invalid_argument if delta_seconds is not finite or is negative.
 */
void AdvanceKinematics(Registry &world, float delta_seconds, Vector2D gravity,
                       AsyncParallelForDriver &driver);

} // namespace svanes

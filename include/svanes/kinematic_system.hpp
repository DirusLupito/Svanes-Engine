#pragma once

#include <svanes/physics_system.hpp>
#include <svanes/vector2d.hpp>

#include <optional>

namespace svanes {

class Registry;
class AsyncParallelForDriver;
struct Transform;

/**
 * Motion integrated into an entity's Transform. Acceleration persists until
 * changed. Limits must be finite and nonnegative; std::nullopt means unlimited.
 *
 * FIELDS:
 * - velocity_x: Horizontal velocity in world units per local tic.
 * - velocity_y: Vertical velocity in world units per local tic.
 *
 * ====
 *
 * - acceleration_x: Horizontal acceleration in world units per local tic
 * squared.
 * - acceleration_y: Vertical acceleration in world units per local tic squared.
 *
 * ====
 *
 * - angular_velocity: Angular velocity in radians per local tic.
 * - angular_acceleration: Angular acceleration in radians per local tic
 * squared.
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
 * - delta_tics: How many local tics to advance the kinematic state.
 * - acceleration: The total acceleration to be applied to the entity, which
 * includes contributions from attractors and global acceleration fields.
 */
struct WorkItem {
    Transform *transform;
    Kinematic2D *motion;
    Vector2D acceleration;
    TicCount delta_tics;
};

/**
 * Represents a gravity force applied to entities that have both the Kinematic2D
 * and Gravity components. Acceleration is in world units per local tic squared.
 * Default is set to {0, 0}.
 *
 * Since we're in screen and world space, positive y is downwards. The gravity
 * vector is applied to the acceleration of entities with Kinematic2D and
 * Gravity components.
 */
struct Gravity {};


/**
 * Advances the kinematic state of all entities in the provided registry
 * according to the local delta tics supplied for this physics step.
 * AdvanceTimelines must run first.
 *
 * Each application physics simulation step advances the shared simulation
 * clock by the same number of source tics. A source tic is one tic on that
 * clock, measured in microseconds here. Timelines may convert that source
 * interval into different numbers of local tics for different entities, but
 * they do not change the source interval. For example, during a step of
 * 10000 source tics, a normal-speed entity receives 10000 local tics, a
 * double-speed entity receives 20000, and a half-speed entity receives 5000.
 * The integration uses each entity's local delta directly.
 *
 * @param world The registry containing all entities and their components.
 * @param gravity The gravity vector to apply to entities with Kinematic2D and
 * Gravity components.
 * @param driver The AsyncParallelForDriver to use for parallel execution of
 * the kinematic updates. If the driver has a concurrency of 1, the updates will
 * be executed sequentially on the calling thread.
 * @param entity_steps The entities and their local elapsed tics for this
 * simulation step. Each entity appears once and has Transform, Kinematic2D,
 * and Timeline components. A delta_tics of zero means that the entity
 * will skip integration and clamping for this step.
 *
 * @throws std::invalid_argument for nonfinite gravity or invalid limits.
 */
void AdvanceKinematics(Registry &world, Vector2D gravity,
                       AsyncParallelForDriver &driver,
                       std::span<const PhysicsTimeStep> entity_steps);

} // namespace svanes

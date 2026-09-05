#include <svanes/kinematic_system.hpp>

#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace svanes {

/**
 * Validates that the provided limit is either std::nullopt or a finite, nonnegative value.
 * 
 * @param limit The optional limit to validate.
 * 
 * @throws std::invalid_argument if the limit is not std::nullopt and is either not finite or negative.
 */
static void ValidateLimit(std::optional<float> limit)
{
    if (limit && (!std::isfinite(*limit) || *limit < 0.0F)) {
        throw std::invalid_argument("Kinematic2D magnitude limits must be finite and nonnegative.");
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
 * @throws std::invalid_argument if the limit is not std::nullopt and is either not finite or negative.
 */
static void ClampMagnitude(float& x, float& y, std::optional<float> limit)
{
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
 * @throws std::invalid_argument if the limit is not std::nullopt and is either not finite or negative.
 */
static void ClampMagnitude(float& value, std::optional<float> limit)
{
    ValidateLimit(limit);
    if (limit) {
        value = std::clamp(value, -*limit, *limit);
    }
}

void AdvanceKinematics(Registry& world, float delta_seconds)
{
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F) {
        throw std::invalid_argument("Kinematics delta_seconds must be finite and nonnegative.");
    }

    // Filter to only update entities that have both a Transform (representing position and rotation)
    // and a Kinematic2D (representing motion state).
    world.ForEach<Transform, Kinematic2D>(
        [delta_seconds](Entity, Transform& transform, Kinematic2D& motion) {
            ClampMagnitude(motion.acceleration_x, motion.acceleration_y, motion.max_acceleration);
            ClampMagnitude(motion.angular_acceleration, motion.max_angular_acceleration);

            motion.velocity_x += motion.acceleration_x * delta_seconds;
            motion.velocity_y += motion.acceleration_y * delta_seconds;
            motion.angular_velocity += motion.angular_acceleration * delta_seconds;

            ClampMagnitude(motion.velocity_x, motion.velocity_y, motion.max_speed);
            ClampMagnitude(motion.angular_velocity, motion.max_angular_speed);

            transform.x += motion.velocity_x * delta_seconds;
            transform.y += motion.velocity_y * delta_seconds;
            transform.rotation += motion.angular_velocity * delta_seconds;
        }
    );
}

}

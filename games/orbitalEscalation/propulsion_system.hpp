#pragma once

#include <svanes/vector2d.hpp>

namespace svanes {
class Registry;
}

/**
 * Represents the propulsion capabilities of an entity, consisting of its
 * acceleration limits and forward direction.
 */
class Propulsion final {
public:
    /**
     * Constructs a Propulsion component with the specified acceleration limits
     * and forward direction.
     *
     * @param max_acceleration The maximum linear acceleration in units per
     * second squared.
     * @param max_angular_acceleration The maximum angular acceleration in
     * radians per second squared.
     * @param forward The forward direction in the entity's local coordinates.
     * Its length need not be 1, the constructor normalizes it.
     *
     * @throws std::invalid_argument if either acceleration limit is not finite
     * or is negative, or if forward has a nonfinite or zero length.
     */
    Propulsion(float max_acceleration, float max_angular_acceleration,
               svanes::Vector2D forward);

    /**
     * Returns the maximum linear acceleration the entity's propulsion can
     * produce.
     *
     * @return The maximum linear acceleration in units per second squared.
     */
    float GetMaxAcceleration() const;

    /**
     * Returns the maximum angular acceleration the entity's propulsion can
     * produce.
     *
     * @return The maximum angular acceleration in radians per second squared.
     */
    float GetMaxAngularAcceleration() const;

    /**
     * Returns the entity's forward direction before applying its rotation.
     *
     * @return A unit vector in the entity's local coordinates. For example,
     * (0, -1) points toward the top of the entity's unrotated geometry.
     */
    svanes::Vector2D GetForward() const;

private:
    // The maximum linear acceleration in units per second squared.
    float max_acceleration;

    // The maximum angular acceleration in radians per second squared.
    float max_angular_acceleration;

    // The unit vector pointing forward in the entity's local coordinates.
    svanes::Vector2D forward;
};

/**
 * Holds the requested propulsion controls.
 *
 * FIELDS:
 * - thrust: A 2D vector representing the thrust direction and magnitude.
 *   The x component corresponds to left/right thrust, and the y component
 *   corresponds to forward/backward thrust, relative to the object's forward
 *   direction. Negative x means left, positive x means right, negative y means
 *   forward, and positive y means backward. The system limits the magnitude
 *   to 1 so diagonal thrust does not exceed the maximum acceleration.
 * - rotation: A float representing the rotational input for the object.
 *   The system limits the value to the range [-1, 1], where -1 represents
 *   full counter-clockwise rotation and 1 represents full clockwise rotation.
 */
struct PropulsionControl {
    svanes::Vector2D thrust;
    float rotation = 0.0F;
};

/**
 * Applies propulsion controls to all entities with the required propulsion,
 * kinematic, transform, and control components.
 *
 * @param world The registry containing the entities to which propulsion is
 * applied.
 *
 * @throws std::invalid_argument if an entity's propulsion controls contain a
 * nonfinite value or its thrust has a nonfinite magnitude.
 */
void ApplyPropulsion(svanes::Registry &world);

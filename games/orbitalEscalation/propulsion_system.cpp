#include "propulsion_system.hpp"

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

// Class methods for Propulsion

Propulsion::Propulsion(float max_acceleration, float max_angular_acceleration,
                       svanes::Vector2D forward)
    : max_acceleration(max_acceleration),
      max_angular_acceleration(max_angular_acceleration), forward(forward) {
    if (!std::isfinite(max_acceleration) || max_acceleration < 0.0F ||
        !std::isfinite(max_angular_acceleration) ||
        max_angular_acceleration < 0.0F) {
        throw std::invalid_argument(
            "Propulsion acceleration limits must be finite and nonnegative");
    }

    const float length = std::hypot(forward.x, forward.y);
    if (!std::isfinite(length) || length == 0.0F) {
        throw std::invalid_argument(
            "Propulsion forward direction must have a finite, nonzero length");
    }

    this->forward /= length;
}

float Propulsion::GetMaxAcceleration() const { return max_acceleration; }

float Propulsion::GetMaxAngularAcceleration() const {
    return max_angular_acceleration;
}

svanes::Vector2D Propulsion::GetForward() const { return forward; }

// End of class methods

void ApplyPropulsion(svanes::Registry &world) {
    world.ForEach<svanes::Transform, svanes::Kinematic2D, Propulsion,
                  PropulsionControl>([](svanes::Entity,
                                        const svanes::Transform &transform,
                                        svanes::Kinematic2D &motion,
                                        const Propulsion &propulsion,
                                        const PropulsionControl &controls) {
        svanes::Vector2D thrust = controls.thrust;
        const float magnitude = std::hypot(thrust.x, thrust.y);

        if (!std::isfinite(magnitude) || !std::isfinite(controls.rotation)) {
            throw std::invalid_argument("Propulsion controls must be finite");
        }

        // If the magnitude of the thrust vector exceeds 1, normalize it to
        // ensure that the thrust values remain within the range [-1, 1].
        if (magnitude > 1.0F) {
            thrust /= magnitude;
        }

        // The entity's forward direction is rotated into world coordinates,
        // then used to turn its requested thrust into linear acceleration.
        // Rotation input determines angular acceleration. These replace the
        // accelerations stored in Kinematic2D, so collision responses should
        // be applied afterward.

        const float cosine = std::cos(transform.rotation);
        const float sine = std::sin(transform.rotation);

        // Forward direction relative to the entity's rotation.
        const svanes::Vector2D local_forward = propulsion.GetForward();

        // Forward direction in world coordinates.
        const svanes::Vector2D forward{
            local_forward.x * cosine - local_forward.y * sine,
            local_forward.x * sine + local_forward.y * cosine};

        // Once we have the forward direction, we can compute the right
        // direction, which we can use if the ship is thrusting left or right
        // (i.e., imagine that in addition to primary thrusters, the ship has
        // small lateral thrusters that can push it left or right).

        const svanes::Vector2D right{-forward.y, forward.x};

        const svanes::Vector2D acceleration =
            (right * thrust.x - forward * thrust.y) *
            svanes::PerSecondSquaredToPerTicSquared(
                propulsion.GetMaxAcceleration());

        motion.acceleration_x = acceleration.x;
        motion.acceleration_y = acceleration.y;

        motion.angular_acceleration =
            std::clamp(controls.rotation, -1.0F, 1.0F) *
            svanes::PerSecondSquaredToPerTicSquared(
                propulsion.GetMaxAngularAcceleration());
    });
}

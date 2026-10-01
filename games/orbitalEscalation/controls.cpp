#include "controls.hpp"

#include <svanes/camera2d.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/input.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

PropulsionControl ReadShipControls(const svanes::InputManager &input,
                                   const svanes::Camera2D &camera,
                                   const svanes::Transform &transform,
                                   const svanes::Kinematic2D &motion,
                                   const Propulsion &propulsion) {

    // WASD for translation, QE for rotation that overrides mouse aiming,
    // mouse aiming otherwise.

    PropulsionControl control{
        .thrust =
            {
                static_cast<float>(input.IsDown(svanes::Key::D) -
                                   input.IsDown(svanes::Key::A)),
                static_cast<float>(input.IsDown(svanes::Key::S) -
                                   input.IsDown(svanes::Key::W)),
            },
        .rotation = static_cast<float>(input.IsDown(svanes::Key::E) -
                                       input.IsDown(svanes::Key::Q)),
    };
    if (input.IsDown(svanes::Key::Q) || input.IsDown(svanes::Key::E)) {
        return control;
    }

    // If the player is not pressing Q or E, mouse aiming determines the desired
    // rotation. The controller uses the critically damped PD formula:
    //
    // acceleration = Kp * error - Kd * angular_velocity
    //
    // Here, error is the shortest signed angle from the ship to the target,
    // angular_velocity is the ship's current rotation rate, Kp is
    // response * response, and Kd is 2 * response. The proportional term
    // turns toward the target, while the derivative term brakes existing
    // rotation to reduce overshoot.
    //
    // Taken from CSC549.

    const auto mouse = input.MousePosition();
    const auto target = camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});
    const svanes::Vector2D direction{target.x - transform.x,
                                     target.y - transform.y};

    const float max_acceleration = propulsion.GetMaxAngularAcceleration();
    if ((direction.x == 0.0F && direction.y == 0.0F) ||
        max_acceleration == 0.0F) {
        return control;
    }

    const auto forward = propulsion.GetForward();
    const float target_rotation =
        std::atan2(direction.y, direction.x) - std::atan2(forward.y, forward.x);

    const float error = std::remainder(target_rotation - transform.rotation,
                                       2.0F * std::numbers::pi_v<float>);

    const float angular_velocity =
        motion.angular_velocity * svanes::TicsPerSecond;
    
    constexpr float response = 6.0F;
    const float acceleration =
        response * response * error - 2.0F * response * angular_velocity;

    // The control system expects a value in the range [-1, 1], so we scale the
    // acceleration by the maximum acceleration and clamp it to that range.
    control.rotation = std::clamp(acceleration / max_acceleration, -1.0F, 1.0F);
    return control;
}

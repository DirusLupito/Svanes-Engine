#include "controls.hpp"

#include <svanes/input.hpp>

#include <cmath>

ShipControls ReadShipControls(const svanes::InputManager &input) {

    // WASD for translation, QE for rotation

    ShipControls controls{
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

    // If the magnitude of the thrust vector exceeds 1, normalize it to ensure
    // that the thrust values remain within the range [-1, 1].

    const float magnitude = std::hypot(controls.thrust.x, controls.thrust.y);
    if (magnitude > 1.0F) {
        controls.thrust /= magnitude;
    }

    return controls;
}

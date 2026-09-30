#include "controls.hpp"

#include <svanes/input.hpp>

PropulsionControl ReadShipControls(const svanes::InputManager &input) {

    // WASD for translation, QE for rotation

    return PropulsionControl{
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
}

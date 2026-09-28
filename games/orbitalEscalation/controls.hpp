#pragma once

#include <svanes/vector2d.hpp>

namespace svanes {
class InputManager;
}

/**
 * Holds the current state of the ship's controls.
 *
 * FIELDS:
 * - Thrust: A 2D vector representing the thrust direction and magnitude.
 *   The x component corresponds to left/right thrust, and the y component
 *   corresponds to forward/backward thrust. The values are normalized to the
 *   range [-1, 1], where -1 represents full thrust in the negative direction
 *   and 1 represents full thrust in the positive direction.
 * - Rotation: A float representing the rotational input for the ship.
 *   The value is normalized to the range [-1, 1], where -1 represents
 *   full counter-clockwise rotation and 1 represents full clockwise rotation.
 */
struct ShipControls {
    svanes::Vector2D thrust;
    float rotation = 0.0F;
};

/**
 * Reads the current state of the ship's controls based on the input manager.
 *
 * This function checks the state of specific keys to determine the thrust
 * and rotation inputs for the ship.
 *
 * @param input The input manager to read the current input state from.
 * @return A ShipControls struct containing the current thrust and rotation
 *         values.
 */
ShipControls ReadShipControls(const svanes::InputManager &input);

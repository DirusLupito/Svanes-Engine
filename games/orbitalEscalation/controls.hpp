#pragma once

#include "propulsion_system.hpp"

namespace svanes {
class InputManager;
}

/**
 * Reads the current state of the ship's controls based on the input manager.
 *
 * This function checks the state of specific keys to determine the thrust
 * and rotation inputs for the ship.
 *
 * @param input The input manager to read the current input state from.
 * @return A PropulsionControl struct containing the current thrust and rotation
 *         values.
 */
PropulsionControl ReadShipControls(const svanes::InputManager &input);

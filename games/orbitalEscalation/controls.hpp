#pragma once

#include "propulsion_system.hpp"

namespace svanes {
class InputManager;
class Camera2D;
struct Transform;
struct Kinematic2D;
}

/**
 * Reads the current state of the ship's controls based on the input manager.
 *
 * This function checks the state of specific keys to determine the thrust
 * and rotation inputs for the ship. Q and E override mouse steering. Otherwise,
 * the ship turns toward the mouse and brakes as it approaches that direction.
 *
 * @param input The input manager to read the current input state from.
 * @param camera The camera used to convert the mouse position to world
 * coordinates.
 * @param transform The ship's current position and rotation.
 * @param motion The ship's current motion, used to brake its rotation.
 * @param propulsion The ship's forward direction and acceleration limits.
 * @return A PropulsionControl struct containing the current thrust and rotation
 *         values.
 */
PropulsionControl ReadShipControls(const svanes::InputManager &input,
                                   const svanes::Camera2D &camera,
                                   const svanes::Transform &transform,
                                   const svanes::Kinematic2D &motion,
                                   const Propulsion &propulsion);

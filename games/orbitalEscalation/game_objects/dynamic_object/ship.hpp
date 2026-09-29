#pragma once

#include "dynamic_object.hpp"

/**
 * Defines the properties of a ship. A ship has acceleration limits, a collision
 * layer, and a set of visuals that represent its appearance in the game world.
 *
 * FIELDS:
 * - max_acceleration: The maximum linear acceleration the ship can achieve, in
 * units per second squared.
 * - max_angular_acceleration: The maximum angular acceleration the ship can
 * achieve, in radians per second squared.
 * - collider: A Collider2D that defines the ship's collision layer.
 * - visuals: A vector of Visual objects that define the ship's appearance in
 * the game world.
 */
struct ShipDefinition {
    float max_acceleration;
    float max_angular_acceleration;
    svanes::Collider2D collider;
    std::vector<Visual> visuals;
};

/**
 * Represents a ship in the game world (basically a dynamic object with a
 * maximum acceleration and maximum angular acceleration).
 */
class Ship final : public DynamicObject {
public:
    /**
     * Constructs a Ship entity in the provided registry.
     *
     * @param world The registry in which to create the ship entity and its
     * components.
     * @param gameplay_timeline The parent timeline entity to which the ship's
     * Timeline will be linked.
     * @param definition The ship's acceleration limits, collider, and visuals.
     */
    Ship(svanes::Registry &world, svanes::Entity gameplay_timeline,
         ShipDefinition definition);

    // The maximum linear acceleration the ship can achieve, in units per second
    // squared.
    float max_acceleration;

    // The maximum angular acceleration the ship can achieve, in radians per
    // second squared.
    float max_angular_acceleration;
};

#pragma once

#include <svanes/entity.hpp>

namespace svanes {
class InputManager;
class Registry;
class TextureManager;
struct Transform;
} // namespace svanes

/**
 * Represents a ship in the game world, consisting of some rendered and
 * collidable geometry, and a kinematic component for movement.
 */
class Ship final {
public:
    /**
     * Constructs a Ship entity in the provided registry.
     *
     * @param world The registry in which to create the ship entity and its
     * components.
     * @param assets The texture manager used to create the ship's texture.
     * @param gameplay_timeline The parent timeline entity to which the ship's
     * Timeline will be linked.
     * @param start The initial transform (position and rotation) of the ship.
     */
    Ship(svanes::Registry &world, svanes::TextureManager &assets,
         svanes::Entity gameplay_timeline, const svanes::Transform &start);

    /**
     * Returns the unique identifier of the ship entity.
     *
     * @return The unique identifier of the ship entity.
     */
    svanes::Entity GetEntity() const;


    void ApplyInput(svanes::Registry &world,
                    const svanes::InputManager &input) const;

    // The maximum linear acceleration the ship can achieve, in units per second
    // squared.
    float max_acceleration = 1000.0F;

    // The maximum angular acceleration the ship can achieve, in degrees per
    // second squared.
    float max_angular_acceleration = 100.0F;

private:
    // The unique identifier of the ship entity in the registry.
    svanes::Entity entity;
};

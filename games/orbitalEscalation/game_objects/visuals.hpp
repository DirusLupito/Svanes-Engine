#pragma once

#include <svanes/entity.hpp>
#include <svanes/render/render_system.hpp>

#include <variant>
#include <vector>

/**
 * Represents a visual component of a game object, which can be a sprite,
 * solid shape, or radial gradient.
 *
 * FIELDS:
 * - component: A variant that can hold either a Sprite, SolidShape, or
 *   RadialGradient2D, representing the visual appearance of the game object.
 * - z_order: An integer representing the rendering order of the visual
 *   component. Higher values are rendered on top of lower values.
 */
struct Visual {
    std::variant<svanes::Sprite, svanes::SolidShape, svanes::RadialGradient2D>
        component;
    std::int32_t z_order = 0;
};

/**
 * Manages the visual representation of a game object in the registry.
 *
 * This class is responsible for creating and updating the visual components
 * of a game object based on its current transform. It holds a collection of
 * entities that represent the visual components in the registry.
 */
class Visuals final {
public:
    /**
     * Constructs a Visuals object and creates the visual components in the
     * registry based on the provided parts.
     *
     * @param world The registry in which to create the visual components.
     * @param parts A vector of Visual objects representing the visual
     * components to create.
     */
    Visuals(svanes::Registry &world, std::vector<Visual> parts);

    /**
     * Updates the transform of the visual components in the registry to match
     * the provided transform.
     *
     * @param world The registry containing the visual components.
     * @param transform The new transform to apply to the visual components.
     */
    void SetTransform(svanes::Registry &world,
                      const svanes::Transform &transform) const;

private:
    // A list of entities in the registry that represent the visual components
    // of the game object.
    std::vector<svanes::Entity> entities;
};

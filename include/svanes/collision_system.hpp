#pragma once

#include <svanes/vector2d.hpp>

#include <optional>

namespace svanes {

struct Transform;

/**
 * Component for 2D entity collision detection.
 * 
 */
struct Collider2D {
};

/**
 * Structure describing the result of a collision detection between two 2D shapes.
 * Represents the direction needed to minimally separate the first shape from the second,
 * and the distance along that direction to reach a non-penetrating state.
 *
 * FIELDS:
 * - normal: Unit direction in which to move the first shape out of the second.
 * - penetration_depth: World-space distance along normal needed to reach touching.
 *   Zero means the shapes already touch without penetrating.
 */
struct Collision2D {
    Vector2D normal;
    float penetration_depth = 0.0F;
};

/**
 * Detects contact between rectangles. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth. 
 *
 * @param a The first rectangle's transform.
 * @param b The second rectangle's transform.
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 * 
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or rectangle edges that cannot be represented with a finite, positive length.
 */
std::optional<Collision2D> DetectCollision(const Transform& a, const Transform& b);

} // namespace svanes

#pragma once

#include <svanes/geometry.hpp>

#include <svanes/vector2d.hpp>

#include <vector>

namespace svanes {

/**
 * Component for 2D entity collision detection.
 * 
 * FIELDS:
 * - geometry: The geometric shape of the entity used for collision detection.
 */
struct Collider2D {
    Geometry2D geometry;
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
 * Detects all collisions between two generic 2D geometries, which may be primitive shapes or composite shapes.
 * The function uses the Separating Axis Theorem to determine if the shapes intersect and calculates the
 * penetration depth and normal of the collision if they do.
 * 
 * @param a The first geometry to test for collisions.
 * @param transform_a The transform to apply to the first geometry.
 * @param b The second geometry to test for collisions.
 * @param transform_b The transform to apply to the second geometry.
 * 
 * @return A vector of Collision2D objects representing the detected collisions. If no collisions are detected, the vector will be empty.
 */
std::vector<Collision2D> DetectCollisions(
    const Geometry2D& a, const Transform& transform_a,
    const Geometry2D& b, const Transform& transform_b
);

} // namespace svanes

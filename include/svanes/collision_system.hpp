#pragma once

#include <svanes/vector2d.hpp>

#include <optional>

namespace svanes {

struct Transform;
struct Rectangle2D;
struct Triangle2D;

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
 * @param a The first rectangle's geometry.
 * @param transform_a The first rectangle's transform.
 * @param b The second rectangle's geometry.
 * @param transform_b The second rectangle's transform.
 * 
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 * 
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or rectangle edges that cannot be represented with a finite, positive length.
 */
std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
);

/**
 * Detects contact between triangles. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth.
 * 
 * @param a The first triangle's vertices.
 * @param b The second triangle's vertices.
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 *
 * @throws std::invalid_argument for triangle edges that cannot be represented with a finite, positive length.
 */
std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
);

/**
 * Detects contact between a triangle and a rectangle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth. 
 *
 * @param a The triangle's vertices.
 * @param b The rectangle's geometry.
 * @param transform_b The rectangle's transform.
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 * 
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or triangle edges that cannot be represented with a finite, positive length.
 */
std::optional<Collision2D> DetectCollision(
    const Triangle2D& a, const Transform& transform_a,
    const Rectangle2D& b, const Transform& transform_b
);

/**
 * Detects contact between a rectangle and a triangle. Describes the nature or lack thereof of
 * the contact as both a normal and a penetration depth. 
 *
 * @param a The rectangle's geometry.
 * @param transform_a The rectangle's transform.
 * @param b The triangle's vertices.
 * 
 * @return Contact information, or std::nullopt when separated. 
 * In the case that two different axes yield the same penetration depth,
 * the first axis encountered is the one returned.
 * 
 * @throws std::invalid_argument for non-finite transforms, nonpositive dimensions,
 * or triangle edges that cannot be represented with a finite, positive length.
 */
std::optional<Collision2D> DetectCollision(
    const Rectangle2D& a, const Transform& transform_a,
    const Triangle2D& b, const Transform& transform_b
);

} // namespace svanes

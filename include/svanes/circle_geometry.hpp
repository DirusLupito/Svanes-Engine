#pragma once

namespace svanes {

struct Transform;

/**
 * Represents a circle defined by its center coordinates and radius in 2D space.
 *
 * FIELDS:
 * - x: The x-coordinate of the circle's center.
 * - y: The y-coordinate of the circle's center.
 * - radius: The radius of the circle. Must be non-negative.
 */
struct Circle2D {
    float x = 0.0F;
    float y = 0.0F;
    float radius = 0.0F;
};

/**
 * Transforms a circle from its local coordinates to world coordinates
 * by applying a translation and rotation defined by a Transform.
 *
 * @param circle The circle in local coordinates.
 * @param transform The transform to apply.
 * @return The circle in world coordinates.
 */
Circle2D TransformCircle(Circle2D circle, const Transform& transform);

}

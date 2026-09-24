#pragma once

#include <svanes/geometry/rectangle_geometry.hpp>

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
Circle2D TransformCircle(Circle2D circle, const Transform &transform);

/**
 * Represents a circle in 2D space and provides methods to get its
 * axis-aligned bounding box.
 */
class CircleGeometry final {
public:
    /**
     * Constructs a CircleGeometry object from a Circle2D.
     *
     * @param circle The circle to be represented by this geometry.
     */
    explicit CircleGeometry(Circle2D circle);

    /**
     * Calculates the axis-aligned bounding box of a circle.
     *
     * @return A Rectangle2D representing the axis-aligned bounding box of the
     * circle.
     */
    Rectangle2D Bounds() const;

private:
    // The circle's center and radius in local coordinates.
    Circle2D circle;
};

} // namespace svanes

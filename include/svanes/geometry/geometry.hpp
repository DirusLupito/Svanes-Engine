#pragma once

#include <svanes/geometry/composite_geometry.hpp>

#include <optional>
#include <variant>

namespace svanes {

// Represents any 2D geometric shape that can be used for rendering or collision
// detection.
using Geometry2D = std::variant<Rectangle2D, Triangle2D, Circle2D,
                                ConvexPolygon2D, CompositeShape2D>;

/**
 * Computes the axis-aligned bounding box of a 2D geometry after applying a
 * transform.
 *
 * @param geometry The 2D geometry to compute the bounding box for.
 * @param transform The transform to apply to the geometry before computing
 * the bounding box.
 *
 * @return An optional Rectangle2D representing the axis-aligned bounding
 * box of the transformed geometry. In the case of a composite shape with
 * no parts, std::nullopt is returned to indicate that there is no bounding
 * box.
 *
 * @throws std::invalid_argument if the transform is not finite or if the
 * geometry is invalid.
 */
std::optional<Rectangle2D> ComputeBounds(const Geometry2D &geometry,
                                         const Transform &transform);

} // namespace svanes

namespace svanes::internal {

/**
 * Computes the axis-aligned bounding box from the given minimum and maximum
 * coordinates. The resulting bounding box is represented as a Rectangle2D
 * with its center at the midpoint of the minimum and maximum coordinates and
 * its width and height equal to the distance between the minimum and maximum
 * coordinates.
 *
 * @param min_x The minimum x-coordinate of the bounding box.
 * @param min_y The minimum y-coordinate of the bounding box.
 * @param max_x The maximum x-coordinate of the bounding box.
 * @param max_y The maximum y-coordinate of the bounding box.
 *
 * @return A Rectangle2D representing the axis-aligned bounding box.
 *
 * @throws std::overflow_error if the computed center or dimensions exceed
 * the representable range of float.
 */
Rectangle2D BoundsFromExtents(double min_x, double min_y, double max_x,
                              double max_y);

/**
 * Computes the axis-aligned bounding box of a geometric primitive after
 * applying a transform. The primitive can be a rectangle, triangle, circle,
 * or convex polygon.
 *
 * @param geometry The geometric primitive to compute the bounding box for.
 * @param transform The transform to apply to the primitive before computing
 * the bounding box.
 *
 * @return A Rectangle2D representing the axis-aligned bounding box of the
 * transformed primitive.
 *
 * @throws std::invalid_argument if the transform is not finite or if the
 * geometry is invalid.
 */
Rectangle2D ComputePrimitiveBounds(const Primitive2D &geometry,
                                   const Transform &transform);

} // namespace svanes::internal

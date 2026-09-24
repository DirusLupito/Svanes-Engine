#include <svanes/geometry/composite_geometry.hpp>
#include <svanes/geometry/geometry.hpp>

#include <cmath>
#include <stdexcept>

namespace svanes {

Transform ComposeTransforms(const Transform &parent, const Transform &local) {
    const float cosine = std::cos(parent.rotation);
    const float sine = std::sin(parent.rotation);
    return {
        parent.x + local.x * cosine - local.y * sine,
        parent.y + local.x * sine + local.y * cosine,
        parent.rotation + local.rotation,
    };
}

/**
 * Small helper function to validate that a transform's x, y, and rotation
 * values are finite.
 *
 * @param transform The transform to validate.
 *
 * @throws std::invalid_argument if the transform is not finite.
 */
static void ValidateBoundsTransform(const Transform &transform) {
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y) ||
        !std::isfinite(transform.rotation)) {
        throw std::invalid_argument(
            "Geometry bounds require a finite transform.");
    }
}

//
//
// bunch of wrappers for an std::visit around the preexisting bounds functions
// for each geometric type, including composite shapes.
//
//

/**
 * Overloaded shape bounds function for Rectangle2D.
 * Computes the axis-aligned bounding box of a rectangle after applying a
 * transform.
 *
 * @param rectangle The rectangle to compute the bounds for.
 * @param transform The transform to apply to the rectangle.
 *
 * @return A Rectangle2D representing the axis-aligned bounding box of the
 * transformed rectangle.
 */
static Rectangle2D ShapeBounds(const Rectangle2D &rectangle,
                               const Transform &transform) {
    return RectangleGeometry(TransformRectangle(rectangle, transform),
                             transform.rotation)
        .Bounds();
}

/**
 * Overloaded shape bounds function for Triangle2D.
 * Computes the axis-aligned bounding box of a triangle after applying a
 * transform.
 *
 * @param triangle The triangle to compute the bounds for.
 * @param transform The transform to apply to the triangle.
 *
 * @return A Rectangle2D representing the axis-aligned bounding box of the
 * transformed triangle.
 */
static Rectangle2D ShapeBounds(const Triangle2D &triangle,
                               const Transform &transform) {
    return TriangleGeometry(TransformTriangle(triangle, transform)).Bounds();
}

/**
 * Overloaded shape bounds function for Circle2D.
 * Computes the axis-aligned bounding box of a circle after applying a
 * transform.
 *
 * @param circle The circle to compute the bounds for.
 * @param transform The transform to apply to the circle.
 *
 * @return A Rectangle2D representing the axis-aligned bounding box of the
 * transformed circle.
 */
static Rectangle2D ShapeBounds(const Circle2D &circle,
                               const Transform &transform) {
    return CircleGeometry(TransformCircle(circle, transform)).Bounds();
}

/**
 * Overloaded shape bounds function for ConvexPolygon2D.
 * Computes the axis-aligned bounding box of a convex polygon after applying a
 * transform.
 *
 * @param polygon The convex polygon to compute the bounds for.
 * @param transform The transform to apply to the convex polygon.
 *
 * @return A Rectangle2D representing the axis-aligned bounding box of the
 * transformed convex polygon.
 */
static Rectangle2D ShapeBounds(const ConvexPolygon2D &polygon,
                               const Transform &transform) {
    return TransformConvexPolygon(polygon, transform).Bounds();
}

/**
 * Overloaded shape bounds function for CompositeShape2D.
 * Computes the axis-aligned bounding box of a composite shape after applying a
 * transform.
 *
 * @param composite The composite shape to compute the bounds for.
 * @param transform The transform to apply to the composite shape.
 *
 * @return A Rectangle2D representing the axis-aligned bounding box of the
 * transformed composite shape, or std::nullopt if it has no parts.
 */
static std::optional<Rectangle2D> ShapeBounds(const CompositeShape2D &composite,
                                              const Transform &transform) {
    return CompositeGeometry(composite).Bounds(transform);
}

Rectangle2D internal::ComputePrimitiveBounds(const Primitive2D &geometry,
                                             const Transform &transform) {
    ValidateBoundsTransform(transform);
    return std::visit(
        [&](const auto &shape) { return ShapeBounds(shape, transform); },
        geometry);
}

std::optional<Rectangle2D> ComputeBounds(const Geometry2D &geometry,
                                         const Transform &transform) {
    ValidateBoundsTransform(transform);
    return std::visit(
        [&](const auto &shape) -> std::optional<Rectangle2D> {
            return ShapeBounds(shape, transform);
        },
        geometry);
}

} // namespace svanes

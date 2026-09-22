#include <svanes/circle_geometry.hpp>

#include <svanes/geometry.hpp>

#include <cmath>
#include <stdexcept>

namespace svanes {

CircleGeometry::CircleGeometry(Circle2D circle) : circle(circle) {}

Rectangle2D CircleGeometry::Bounds() const {
    // To find the axis-aligned bounding box of a circle,
    // we simply subtract and add the radius to the center coordinates.
    // The bounding box's top-left corner will be at (x - radius, y - radius)
    // and its width and height will both be twice the radius.
    if (!std::isfinite(circle.x) || !std::isfinite(circle.y) ||
        !std::isfinite(circle.radius) || circle.radius <= 0.0F) {
        throw std::invalid_argument("Circle bounds require a finite center and "
                                    "a finite, positive radius.");
    }

    const float diameter = circle.radius * 2.0F;
    if (!std::isfinite(diameter)) {
        throw std::overflow_error(
            "Circle bounds exceed the representable dimensions.");
    }

    return {circle.x, circle.y, diameter, diameter};
}

Circle2D TransformCircle(Circle2D circle, const Transform &transform) {
    if (!std::isfinite(circle.x) || !std::isfinite(circle.y) ||
        !std::isfinite(circle.radius) || circle.radius <= 0.0F ||
        !std::isfinite(transform.x) || !std::isfinite(transform.y) ||
        !std::isfinite(transform.rotation)) {
        throw std::invalid_argument(
            "Circles require finite centers and transforms and a finite, "
            "positive radius.");
    }

    // If the circle's local origin is at its center,
    // then rotation does not affect the circle's position in world coordinates.

    const float cosine = std::cos(transform.rotation);
    const float sine = std::sin(transform.rotation);

    const Vector2D center{circle.x, circle.y};

    circle.x = transform.x + center.x * cosine - center.y * sine;
    circle.y = transform.y + center.x * sine + center.y * cosine;

    if (!std::isfinite(circle.x) || !std::isfinite(circle.y)) {
        throw std::invalid_argument(
            "Transformed circle centers must be finite.");
    }
    return circle;
}

} // namespace svanes

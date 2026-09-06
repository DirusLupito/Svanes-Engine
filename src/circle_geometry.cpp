#include <svanes/circle_geometry.hpp>

#include <svanes/render/render_system.hpp>

#include <cmath>
#include <stdexcept>

namespace svanes {

Circle2D TransformCircle(Circle2D circle, const Transform& transform)
{
    if (!std::isfinite(circle.x) || !std::isfinite(circle.y) ||
        !std::isfinite(circle.radius) || circle.radius <= 0.0F ||
        !std::isfinite(transform.x) || !std::isfinite(transform.y) || !std::isfinite(transform.rotation)) {
        throw std::invalid_argument("Circles require finite centers and transforms and a finite, positive radius.");
    }

    // If the circle's local origin is at its center, 
    // then rotation does not affect the circle's position in world coordinates.

    const float cosine = std::cos(transform.rotation);
    const float sine = std::sin(transform.rotation);

    const Vector2D center{circle.x, circle.y};

    circle.x = transform.x + center.x * cosine - center.y * sine;
    circle.y = transform.y + center.x * sine + center.y * cosine;

    if (!std::isfinite(circle.x) || !std::isfinite(circle.y)) {
        throw std::invalid_argument("Transformed circle centers must be finite.");
    }
    return circle;
}

}

#include <svanes/rectangle_geometry.hpp>

#include <svanes/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace svanes {

Rectangle2D internal::BoundsFromExtents(double min_x, double min_y,
                                        double max_x, double max_y) {
    if (min_x > max_x) {
        std::swap(min_x, max_x);
    }

    if (min_y > max_y) {
        std::swap(min_y, max_y);
    }

    const double center_x = (min_x + max_x) * 0.5;
    const double center_y = (min_y + max_y) * 0.5;
    if (!std::isfinite(center_x) || !std::isfinite(center_y) ||
        std::abs(center_x) > std::numeric_limits<float>::max() ||
        std::abs(center_y) > std::numeric_limits<float>::max()) {
        throw std::overflow_error(
            "Geometry bounds exceed the representable coordinates.");
    }

    const double width = max_x - min_x;
    const double height = max_y - min_y;
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width > std::numeric_limits<float>::max() ||
        height > std::numeric_limits<float>::max()) {
        throw std::overflow_error(
            "Geometry bounds exceed the representable dimensions.");
    }

    const float x = static_cast<float>(center_x);
    const float y = static_cast<float>(center_y);

    // The midpoint is equally distant from min and max BEFORE rounding.
    // But storing the center and dimension as two floats can move an edge
    // slightly inward. For example, consider

    // minimum = 1.00000000000000000000000,
    //
    // maximum = 1.00000011920928955078125
    //
    // Then center  = 1.000000059604644775390625, but this cannot be represented
    // as a float, and it will round to 1.0, meaning that we lost a tiny amount
    // of the bounding box. As floating values increase, so too will the
    // magnitude of the rounding error. It's probably not a big deal right now,
    // but if that becomes a problem, use twice the larger distance from the
    // rounded center to either endpoint, then round the dimension upward to a
    // representable float.
    return {x, y, static_cast<float>(width), static_cast<float>(height)};
}

Rectangle2D TransformRectangle(Rectangle2D rectangle,
                               const Transform &transform) {
    const float cosine = std::cos(transform.rotation);
    const float sine = std::sin(transform.rotation);

    const Vector2D center{rectangle.x, rectangle.y};

    // If the rectangle's geometry is such that is is centered at its own
    // origin, then this simplies to just tx + cx and ty + cy.

    // x = tx + cx cos(theta) - cy sin(theta)
    rectangle.x = transform.x + center.x * cosine - center.y * sine;

    // y' = ty + cx sin(theta) + cy cos(theta)
    rectangle.y = transform.y + center.x * sine + center.y * cosine;
    return rectangle;
}

RectangleGeometry::RectangleGeometry(Rectangle2D rectangle, float rotation)
    : half_width(rectangle.width * 0.5F), half_height(rectangle.height * 0.5F),
      center_x(rectangle.x), center_y(rectangle.y), cosine(std::cos(rotation)),
      sine(std::sin(rotation)) {}

Rectangle2D RectangleGeometry::Bounds() const {

    // theta = the rotation of the rectangle
    // a = rectangle's heigh
    // b = rectangle's width
    // x = height of the bounding box
    // y = width of the bounding box
    // (u, v) = the top left corner of the rectangle
    // (p, q) = the top left corner of the bounding box


    // We could solve for the full width and height
    // directly, but then if we want to solve for the
    // top left corner of the bounding box from the
    // top left corner of the rectangle, we would have to
    // figure out the opposite side length and do some
    // soh cah toa to figure out what to add.

    // Instead, we can still do some soh cah toa, but a
    // lesser amount. We solve for the half width and half height of the
    // bounding box then since we know the center of the rectangle is the same
    // as the center of the bounding box, we can just subtract the half width
    // and half height from the center to get the top left corner of the
    // bounding box.

    // So this only leaves unsolved the half width and half height of the
    // bounding box. The reader is recommended to draw a diagram of a rectangle
    // and its bounding box to understand the following:

    // The height of the bounding box is given by
    // x = b sin theta + a cos theta
    // The width of the bounding box is given by
    // y = b cos theta + a sin theta

    // Now we can take the half height and half width and add them to the center
    // of the rectangle to get the top left corner of the bounding box.

    // Another simpler idea is to use a bounding circle. For mostly circular
    // shapes/ rectangles that are mostly square, this will be pretty good. But
    // for long thin rectangles, it would be far more conservative about what is
    // being culled. A benefit of the bounding circle is that once the bound is
    // computed, it need not be recomputed for different rotations, since the
    // bounding circle is rotation invariant.


    if (!std::isfinite(center_x) || !std::isfinite(center_y) ||
        !std::isfinite(half_width) || !std::isfinite(half_height) ||
        !std::isfinite(cosine) || !std::isfinite(sine) || half_width <= 0.0F ||
        half_height <= 0.0F) {
        throw std::invalid_argument("Rectangle bounds require finite "
                                    "coordinates and positive dimensions.");
    }

    const float x_cosine = half_width * std::abs(cosine);
    const float x_sine = half_width * std::abs(sine);
    const float y_cosine = half_height * std::abs(cosine);
    const float y_sine = half_height * std::abs(sine);


    return internal::BoundsFromExtents(
        center_x - x_cosine - y_sine, center_y - x_sine - y_cosine,
        center_x + x_cosine + y_sine, center_y + x_sine + y_cosine);
}

std::array<Vector2D, 4> RectangleGeometry::Corners() const {
    // The rectangle's center stays in the same place
    // at (center_x, center_y) while the four corners
    // are rotated around that center point.

    // We initialize the vertices as relative positions
    // to the center of the rectangle.

    std::array<Vector2D, 4> vertices{{
        // Top left
        {-half_width, -half_height},

        // Top right
        {half_width, -half_height},

        // Bottom right
        {half_width, half_height},

        // Bottom left
        {-half_width, half_height},
    }};

    for (Vector2D &vertex : vertices) {
        const Vector2D offset = vertex;

        // Given our center point (center_x, center_y), the angle of a given
        // vertex from the center will be given by atan2(offset.y, offset.x)

        // We can then just add the rotation to that angle, which gives us
        // the new angle of the vertex from the center.

        // Take     phi   = atan2(offset.y, offset.x)
        // and take theta = the rectangle's rotation
        // Then the new position of the vertex will be given by:

        // x' = r * cos(phi + theta)
        // y' = r * sin(phi + theta)

        // Where r = sqrt(offset.x^2 + offset.y^2) is the distance from the center.
        // Recall:
        // cos(x + y) = cos(x)cos(y) - sin(x)sin(y)
        // sin(x + y) = sin(x)cos(y) + cos(x)sin(y)
        // x = r * cos(atan2(y, x))
        // y = r * sin(atan2(y, x)).

        // So we have
        // x' = r * cos(phi + theta)
        //    = r * (cos(phi)cos(theta) - sin(phi)sin(theta))
        //    = x * cos(theta) - y * sin(theta)

        // y' = r * sin(phi + theta)
        //    = r * (sin(phi)cos(theta) + cos(phi)sin(theta))
        //    = x * sin(theta) + y * cos(theta)

        // We can then just add the center point back to get the final position
        // of the vertex.

        vertex = {
            center_x + offset.x * cosine - offset.y * sine,
            center_y + offset.x * sine + offset.y * cosine,
        };
    }

    return vertices;
}

} // namespace svanes

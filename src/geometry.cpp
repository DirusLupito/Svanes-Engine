#include <svanes/geometry.hpp>

#include <cmath>

namespace svanes {

RectangleGeometry::RectangleGeometry(Rectangle2D rectangle, float rotation)
    : half_width(rectangle.width * 0.5F),
      half_height(rectangle.height * 0.5F),
      center_x(rectangle.x + half_width),
      center_y(rectangle.y + half_height),
      cosine(std::cos(rotation)),
      sine(std::sin(rotation))
{
}

Rectangle2D RectangleGeometry::Bounds() const
{

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
    // lesser amount. We solve for the half width and half height of the bounding box
    // then since we know the center of the rectangle is the same as the center of the
    // bounding box, we can just subtract the half width and half height from the center
    // to get the top left corner of the bounding box.

    // So this only leaves unsolved the half width and half height of the bounding box.
    // The reader is recommended to draw a diagram of a rectangle and its bounding box 
    // to understand the following:

    // The height of the bounding box is given by
    // x = b sin theta + a cos theta
    // The width of the bounding box is given by
    // y = b cos theta + a sin theta

    // Now we can take the half height and half width and add them to the center of the rectangle
    // to get the top left corner of the bounding box.

    // Another simpler idea is to use a bounding circle. For mostly circular shapes/
    // rectangles that are mostly square, this will be pretty good. But for long thin rectangles,
    // it would be far more conservative about what is being culled.
    // A benefit of the bounding circle is that once the bound is computed, it need not be recomputed for different rotations,
    // since the bounding circle is rotation invariant.


    const float extent_x = half_width * std::abs(cosine) + half_height * std::abs(sine);
    const float extent_y = half_width * std::abs(sine) + half_height * std::abs(cosine);
    return {center_x - extent_x, center_y - extent_y, extent_x * 2.0F, extent_y * 2.0F};
}

std::array<Vector2D, 4> RectangleGeometry::Corners() const
{
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

    for (Vector2D& vertex : vertices) {
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

        // We can then just add the center point back to get the final position of the vertex.

        vertex = {
            center_x + offset.x * cosine - offset.y * sine,
            center_y + offset.x * sine + offset.y * cosine,
        };
    }

    return vertices;
}

}

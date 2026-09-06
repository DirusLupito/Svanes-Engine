#include <svanes/triangle_geometry.hpp>

#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace svanes {

/**
 * Validates that a triangle's vertices are finite and non-collinear.
 *
 * @param triangle The triangle to validate.
 *
 * @throws std::invalid_argument if any vertex is non-finite or if the vertices are collinear.
 */
static void ValidateTriangle(const Triangle2D& triangle)
{
    for (Vector2D vertex : triangle.vertices) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)) {
            throw std::invalid_argument("Triangles require finite vertices.");
        }
    }

    const auto& vertices = triangle.vertices;

    // a really big triangle can have massive machine epsilon with floats so
    // we use doubles to compute the area and check for collinearity.
    const double ab_x = static_cast<double>(vertices[1].x) - vertices[0].x;
    const double ab_y = static_cast<double>(vertices[1].y) - vertices[0].y;
    const double ac_x = static_cast<double>(vertices[2].x) - vertices[0].x;
    const double ac_y = static_cast<double>(vertices[2].y) - vertices[0].y;
    if (ab_x * ac_y - ab_y * ac_x == 0.0) {
        throw std::invalid_argument("Triangles require three non-collinear vertices.");
    }
}

Triangle2D TransformTriangle(Triangle2D triangle, const Transform& transform)
{
    ValidateTriangle(triangle);
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y) || !std::isfinite(transform.rotation)) {
        throw std::invalid_argument("Triangles require finite transforms.");
    }
    const float cosine = std::cos(transform.rotation);
    const float sine = std::sin(transform.rotation);
    for (Vector2D& vertex : triangle.vertices) {
        const Vector2D local = vertex;
        vertex = {
            transform.x + local.x * cosine - local.y * sine,
            transform.y + local.x * sine + local.y * cosine,
        };
    }
    ValidateTriangle(triangle);
    return triangle;
}

TriangleGeometry::TriangleGeometry(Triangle2D triangle)
    : vertices(triangle.vertices)
{
}

const std::array<Vector2D, 3>& TriangleGeometry::Vertices() const
{
    return vertices;
}

Rectangle2D TriangleGeometry::Bounds() const
{
    // To find the axis-aligned bounding box of a triangle, 
    // we simply find the minimum and maximum x and y coordinates
    // among the triangle's vertices. The bounding box's top-left corner
    // will be at (min_x, min_y) and its width and height will be
    // (max_x - min_x) and (max_y - min_y), respectively.

    // Simpler than the derivation of the rectangle's bounding box.
    // Although this same approach could be used for the rectangle's
    // bounding box, it would be less efficient than the approach used
    // in RectangleGeometry::Bounds() since it would require computing
    // the four corners of the rectangle and then finding the min and max
    // x and y coordinates among those corners rather than directly
    // computing the min and max x and y coordinates from the rectangle's
    // center, width, height, and rotation.

    Vector2D minimum = vertices[0];
    Vector2D maximum = minimum;

    for (Vector2D vertex : vertices) {
        minimum.x = std::min(minimum.x, vertex.x);
        minimum.y = std::min(minimum.y, vertex.y);
        maximum.x = std::max(maximum.x, vertex.x);
        maximum.y = std::max(maximum.y, vertex.y);
    }

    return {
        minimum.x * 0.5F + maximum.x * 0.5F,
        minimum.y * 0.5F + maximum.y * 0.5F,
        maximum.x - minimum.x, maximum.y - minimum.y,
    };
}

}

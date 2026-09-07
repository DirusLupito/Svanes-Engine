#include <svanes/convex_polygon_geometry.hpp>

#include <svanes/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace svanes {

ConvexPolygon2D::ConvexPolygon2D(std::vector<Vector2D> vertices)
    : vertices(std::move(vertices))
{

    // One may think to check for convexity by checking that all the edges of the polygon have the same winding direction
    // (i.e., all edges turn further clockwise or all edges turn further counter-clockwise).
    // However, we can't just check and see that every pair of successive edges has a clockwise or counter-clockwise winding,
    // because we could have a situation where we have self intersecting edges that still have the same winding.
    // 
    // Instead, think about this: If I taken ANY edge of a convex polygon, and then I extend it infinitely in both directions,
    // the polygon will be entirely on one side or the other of that line. Now let me introduce a new notion.
    // 
    // Denote
    // 
    // vertex[i] = (x[i], y[i])
    //
    // edge[i] = vertex[i+1] - vertex[i] = (x[i+1] - x[i], y[i+1] - y[i])
    // 
    // offset[j] = vertex[j] - vertex[i] = (x[j] - x[i], y[j] - y[i])
    //
    // The 2D cross product of edge[i] and the offset from vertex[i] to vertex[j] is given by:
    //
    //    edge[i] x offset[j] = edge[i].x * offset[j].y - edge[i].y * offset[j].x
    //                        = (x[i+1] - x[i]) * (y[j] - y[i]) - (y[i+1] - y[i]) * (x[j] - x[i])
    // 
    // Why is the parallelogram area positive/negative?
    // Consider the first edge, edge[0] = vertex[1] - vertex[0]. If we rotate edge[0] 90 degrees counter-clockwise, we get a vector
    // perpendicular to edge[0] that points "outward" from the polygon (or inward depending on the orientation). 
    // If we take the dot product of this perpendicular vector with edge[1], 
    // we get a positive value if edge[1] is pointing in the same general direction as the perpendicular vector, 
    // and a negative value if edge[1] is pointing in the opposite direction. 
    // The 2D cross product is equivalent to this dot product, so it has the same sign. Thereby letting us glimpse the orientation of the edges
    // without having to compute the perpendicular vector explicitly.
    // 
    // So this gives us a tool to check for the orientation of every vertex with the vertex i that (in conjunction with vertex i+1) defines the edge.
    // If the offset of vertex j for any j != i, i+1 is on the same side of the line defined by edge[i], i.e. their cross product has the same sign, 
    // then we know that every vertex is on the same side of the line defined by edge[i]. Repeating this for every edge and ensuring the sign
    // is consistent across all edges tested (i.e. if edge 1 says all vertices are on the left, then even if edge 2 says all vertices are on the right,
    // we still have a problem), we can conclude that the polygon is convex. If even a single vertex is on the opposite side of even a single edge,
    // then the polygon is not convex.
    // 
    // Also, while collinearity is not a problem for convexity, it is pointless to have collinear vertices in a polygon, as you
    // may as well just remove the middle vertex. So we will also check for collinearity and throw an exception if we find any.

    const auto& points = this->vertices;
    if (points.size() < 3) {
        throw std::invalid_argument("Convex polygons require at least three vertices.");
    }
    for (Vector2D point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            throw std::invalid_argument("Convex polygons require finite vertices.");
        }
    }

    // Tracks the winding direction of the polygon. We simply record the first nonzero cross product we find,
    // and then check that all subsequent cross products have the same sign. The only relevance of the magnitude
    // is whether it is zero so we can detect collinearity. Double reduces the effect of machine epsilon errors
    // for particularly large or small polygons.
    double winding = 0.0;

    // Outer loop iterates over each edge of the polygon, defined by consecutive vertices i and i+1.
    for (std::size_t i = 0; i < points.size(); ++i) {
        const std::size_t next = (i + 1) % points.size();
        const double edge_x = static_cast<double>(points[next].x) - points[i].x;
        const double edge_y = static_cast<double>(points[next].y) - points[i].y;

        // Inner loop iterates over every other vertex j of the polygon, 
        // checking its position relative to the edge defined by vertices i and i+1.
        for (std::size_t j = 0; j < points.size(); ++j) {
            if (j == i || j == next) {
                continue;
            }

            const double offset_x = static_cast<double>(points[j].x) - points[i].x;
            const double offset_y = static_cast<double>(points[j].y) - points[i].y;
            const double cross = edge_x * offset_y - edge_y * offset_x;

            if (cross == 0.0) {
                throw std::invalid_argument("Convex polygon edges require distinct vertices and no collinear boundary triples.");
            }

            if (winding == 0.0) {
                winding = cross;
            } else if ((cross > 0.0) != (winding > 0.0)) {
                throw std::invalid_argument("Convex polygon vertices must follow a strictly convex boundary in order.");
            }
        }
    }
}

const std::vector<Vector2D>& ConvexPolygon2D::Vertices() const
{
    return vertices;
}

Rectangle2D ConvexPolygon2D::Bounds() const
{
    // Similar to the triangle case, we just find the minimum and maximum x and y coordinates 
    // of the polygon's vertices to determine the axis-aligned bounding box.
    Vector2D minimum = vertices.front();
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

ConvexPolygon2D TransformConvexPolygon(
    const ConvexPolygon2D& polygon, const Transform& transform, float scale
)
{
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y) ||
        !std::isfinite(transform.rotation) || !std::isfinite(scale) || scale <= 0.0F) {
        throw std::invalid_argument("Convex polygons require finite transforms and a finite, positive scale.");
    }

    const float cosine = std::cos(transform.rotation);
    const float sine = std::sin(transform.rotation);

    std::vector<Vector2D> vertices = polygon.Vertices();

    for (Vector2D& vertex : vertices) {
        const Vector2D local = vertex;
        vertex = {
            transform.x + (local.x * cosine - local.y * sine) * scale,
            transform.y + (local.x * sine + local.y * cosine) * scale,
        };
    }

    return ConvexPolygon2D{std::move(vertices)};
}

}

#pragma once

#include <svanes/rectangle_geometry.hpp>
#include <svanes/vector2d.hpp>

#include <vector>

namespace svanes {

struct Transform;

/**
 * Represents a convex polygon in 2D space, defined by a set of vertices.
 * The vertices are expected to be provided in clockwise or counter-clockwise order.
 */
class ConvexPolygon2D final {
public:

    /**
     * Constructs a ConvexPolygon2D with the given vertices, checking to make sure they form a valid convex polygon.
     * 
     * @param vertices A vector of Vector2D representing the vertices of the polygon.
     * The vertices should be provided in clockwise or counter-clockwise order.
     * 
     * @throws std::invalid_argument if there are fewer than three vertices, if any vertex is not finite, 
     * if there is a collinear triple of vertices, or if the vertices do not form a strictly convex polygon.
     */
    explicit ConvexPolygon2D(std::vector<Vector2D> vertices);

    /**
     * Returns the vertices of the convex polygon.
     * 
     * @return A const reference to a vector of Vector2D representing the vertices of the polygon.
     */
    const std::vector<Vector2D>& Vertices() const;

    /**
     * Returns the axis-aligned bounding box of the convex polygon.
     * 
     * @return A Rectangle2D representing the axis-aligned bounding box of the polygon.
     */
    Rectangle2D Bounds() const;

private:
    // The vertices of the convex polygon, stored in the order they were provided.
    std::vector<Vector2D> vertices;
};

/**
 * Transforms a ConvexPolygon2D by applying a translation, rotation, and optional scaling.
 * 
 * @param polygon The ConvexPolygon2D to transform.
 * @param transform The Transform to apply, which includes translation and rotation.
 * @param scale An optional scaling factor to apply to the polygon. Default is 1.0F (no scaling).
 * 
 * @return A new ConvexPolygon2D that is the result of applying the transform to the original polygon.
 * 
 * @throws std::invalid_argument if the transform has non-finite values or if the scale is not positive.
 */
ConvexPolygon2D TransformConvexPolygon(
    const ConvexPolygon2D& polygon, const Transform& transform, float scale = 1.0F
);

}

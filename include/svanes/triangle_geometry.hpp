#pragma once

#include <svanes/rectangle_geometry.hpp>
#include <svanes/vector2d.hpp>

#include <array>

namespace svanes {

struct Transform;

/**
 * Represents a triangle defined by its three vertices in 2D space.
 * Each vertex is represented as a Vector2D (x, y).
 * The three vertices must not be collinear to form a valid triangle.
 *
 * FIELDS:
 * - vertices: An array of three Vector2D objects representing the triangle's vertices.
 */
struct Triangle2D {
    std::array<Vector2D, 3> vertices;
};

/**
 * Transforms a triangle from its local coordinates to world coordinates
 * by applying a translation and rotation defined by a Transform.
 * 
 * The triangle specifies its three vertices in local coordinates.
 * 
 * The transform specifies where the triangle's vertices should be placed in
 * world coordinates and how they should be rotated around the local origin 
 * of the triangle's geometry. The local origin need not be the centroid of
 * the triangle.
 * 
 * @param triangle The triangle to be transformed, defined by its three vertices.
 * @param transform The Transform specifying the translation and rotation to apply.
 * 
 * @return The triangle in world coordinates.
 */
Triangle2D TransformTriangle(Triangle2D triangle, const Transform& transform);

class TriangleGeometry final {
public:

    /**
     * Constructs a TriangleGeometry object from a Triangle2D.
     * 
     * @param triangle The triangle to be represented by this geometry.
     */
    explicit TriangleGeometry(Triangle2D triangle);

    /**
     * Calculates the axis-aligned bounding box of a triangle.
     * 
     * @return A Rectangle2D representing the axis-aligned bounding box of the triangle.
     */
    Rectangle2D Bounds() const;

    /**
     * Returns the triangle's vertices in local coordinates.
     * 
     * @return An array of three Vector2D objects representing the triangle's vertices.
     */
    const std::array<Vector2D, 3>& Vertices() const;

private:

    // The triangle's vertices in local coordinates.
    std::array<Vector2D, 3> vertices;
};

}

#pragma once

#include <svanes/vector2d.hpp>

#include <array>

namespace svanes {

struct Transform;

/**
 * Represents a rectangle defined by its center (x, y) and its dimensions (width, height).
 * All values are floating-point numbers. Units may not necessarily be pixels.
 *
 * FIELDS:
 * - x: The x-coordinate of the rectangle's center.
 * - y: The y-coordinate of the rectangle's center.
 * - width: The width of the rectangle.
 * - height: The height of the rectangle.
 */
struct Rectangle2D {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

/**
 * Transforms a rectangle from its local coordinates to world coordinates
 * by applying a translation and rotation defined by a Transform.
 * 
 * The rectangle specifies its center (x, y) and dimensions (width, height). 
 * The transform specifies where the rectangle's center should be placed in
 * world coordinates and how it should be rotated around that center.
 * 
 * @param rectangle The rectangle to be transformed, defined by its center and dimensions.
 * @param transform The Transform specifying the translation and rotation to apply.
 * 
 * @return The rectangle in world coordinates.
 */
Rectangle2D TransformRectangle(Rectangle2D rectangle, const Transform& transform);

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

class RectangleGeometry final {
public:

    /**
     * Constructs a RectangleGeometry object from a rectangle and a rotation angle.
     * The rectangle is defined by its center (x, y), width, and height.
     * The rotation angle is specified in radians and is applied around the center of the rectangle.
     * 
     * @param rectangle The rectangle to be represented by this geometry.
     */
    RectangleGeometry(Rectangle2D rectangle, float rotation);

    /**
     * Calculates the axis-aligned bounding box of the rectangle after applying the rotation.
     * 
     * That is, given our rectangle and rotation, this function computes the smallest rectangle
     * that can contain the rotated rectangle while having its left two corners differ only in the
     * y-coordinate and its right two corners differ only in the y-coordinate.
     * Or alternatively, its top two corners differ only in the x-coordinate and 
     * its bottom two corners differ only in the x-coordinate.
     * 
     * @return A Rectangle2D representing the axis-aligned bounding box of the rotated rectangle.
     * 
     */
    Rectangle2D Bounds() const;

    /**
     * Returns the four corners of the rectangle after applying the rotation.
     * The corners are returned in the following order: top-left, top-right, bottom-right, bottom-left.
     * 
     * @return An array of four Vector2D objects representing the corners of the rectangle.
     */
    std::array<Vector2D, 4> Corners() const;

private:

    // Half the width of the rotated rectangle.
    float half_width;

    
    float half_height;
    float center_x;
    float center_y;
    float cosine;
    float sine;
};

}

#pragma once

#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <array>

namespace svanes {

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
     * The rectangle is defined by its top-left corner (x, y), width, and height.
     * The rotation angle is specified in radians and is applied around the center of the rectangle.
     * 
     * @param rectangle The rectangle to be represented by this geometry.
     */
    RectangleGeometry(Rectangle rectangle, float rotation);

    /**
     * Calculates the axis-aligned bounding box of the rectangle after applying the rotation.
     * 
     * That is, given our rectangle and rotation, this function computes the smallest rectangle
     * that can contain the rotated rectangle while having its left two corners differ only in the
     * y-coordinate and its right two corners differ only in the y-coordinate.
     * Or alternatively, its top two corners differ only in the x-coordinate and 
     * its bottom two corners differ only in the x-coordinate.
     * 
     * @return A Rectangle representing the axis-aligned bounding box of the rotated rectangle.
     * 
     */
    Rectangle Bounds() const;

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

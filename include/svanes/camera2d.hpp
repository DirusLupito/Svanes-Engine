#pragma once

#include <svanes/circle_geometry.hpp>
#include <svanes/convex_polygon_geometry.hpp>

#include <svanes/rectangle_geometry.hpp>
#include <svanes/triangle_geometry.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <optional>

namespace svanes {

struct Transform;

/**
 * Controls how entity sizes and positions are interpreted relative to the
 * current window size.
 *
 * MEMBERS:
 * - Constant: Pixel values are used verbatim, regardless of window size.
 * - Proportional: Pixel values are rescaled so their proportion of the
 *   screen stays constant across window sizes.
 */
enum class ScaleMode : std::uint8_t {
    Constant,
    Proportional,
};

class Camera2D final {
public:
    // x axis position of the top-left corner of the camera in world coordinates
    float x = 0.0F;

    // y axis position of the top-left corner of the camera in world coordinates
    float y = 0.0F;

    // zoom factor of the camera, where 1.0 means no zoom,
    // > 1.0 means zoomed in, and < 1.0 means zoomed out
    float zoom = 1.0F;

    // how entity sizes and positions are scaled relative to the current window size
    ScaleMode scale_mode = ScaleMode::Constant;

    /**
     * Sets the size of the rendering output the camera draws into,
     * which the scale, offset, and viewport are derived from.
     *
     * @param width The width of the rendering output.
     * @param height The height of the rendering output.
     */
    void SetOutputSize(std::int32_t width, std::int32_t height);

    /**
     * @return The width of the rendering output.
     */
    std::int32_t OutputWidth() const;

    /**
     * @return The height of the rendering output.
     */
    std::int32_t OutputHeight() const;

    /**
     * The factor by which entity sizes and positions are scaled to fit the
     * current output size. In Constant scale mode, or when the output size
     * is empty, this is always 1.0.
     *
     * @return The scale factor applied during rendering.
     */
    float Scale() const;

    /**
     * The distance from the edges of the rendering output to the edges of the
     * scaled game world, used to keep that world centered in the output.
     * In Constant scale mode, or when the output size is empty,
     * this is always {0, 0}.
     *
     * @return The offset applied during rendering.
     */
    Vector2D Offset() const;

    /**
     * The region of the rendering output the game world is drawn into,
     * centered in the output and shrunk by the offset on each side.
     *
     * @return The viewport in screen coordinates.
     */
    Rectangle2D Viewport() const;

    /**
     * Sets the zoom factor of the camera, keeping the specified screen coordinates
     * anchored to the same world coordinates.
     * 
     * @param new_zoom The new zoom factor to set. Must be finite and greater than zero.
     * @param screen_position The position in screen space to anchor.
     *
     * @throws std::invalid_argument if the new zoom factor is not finite or is less than or equal to zero.
     */
    void SetZoomAt(float new_zoom, Vector2D screen_position);

    /**
     * Converts a rectangle from world coordinates to screen coordinates.
     * 
     * @param world The rectangle in world coordinates.
     *
     * @return The rectangle in screen coordinates.
     */
    Rectangle2D WorldToScreen(Rectangle2D world) const;

    /**
     * Converts a rectangle from screen coordinates to world coordinates.
     * 
     * @param screen The rectangle in screen coordinates.
     *
     * @return The rectangle in world coordinates.
     * 
     * @throws std::invalid_argument if the zoom factor is not finite or is less than or equal to zero.
     */
    Rectangle2D ScreenToWorld(Rectangle2D screen) const;

    /**
     * Prepares a rectangular entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the rectangle is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param rectangle The Rectangle2D component of the entity to be rendered.
     *
     * @return An optional Rectangle2D representing the destination rectangle in screen coordinates,
     * or std::nullopt if the rectangle is outside the bounds of the rendering output.
     *
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<Rectangle2D> PrepareForRendering(
        const Transform& transform, const Rectangle2D& rectangle
    ) const;

    /**
     * Prepares a triangular entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the triangle is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param triangle The Triangle2D component of the entity to be rendered.
     *
     * @return An optional Triangle2D representing the destination triangle in screen coordinates,
     * or std::nullopt if the triangle is outside the bounds of the rendering output.
     * 
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<Triangle2D> PrepareForRendering(
        const Transform& transform, const Triangle2D& triangle
    ) const;

    /**
     * Prepares a circular entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the circle is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param circle The Circle2D component of the entity to be rendered.
     *
     * @return An optional Circle2D representing the destination circle in screen coordinates,
     * or std::nullopt if the circle is outside the bounds of the rendering output.
     * 
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<Circle2D> PrepareForRendering(
        const Transform& transform, const Circle2D& circle
    ) const;

    /**
     * Prepares a convex polygon entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the polygon is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param polygon The ConvexPolygon2D component of the entity to be rendered.
     *
     * @return An optional ConvexPolygon2D representing the destination polygon in screen coordinates,
     * or std::nullopt if the polygon is outside the bounds of the rendering output.
     * 
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<ConvexPolygon2D> PrepareForRendering(
        const Transform& transform, const ConvexPolygon2D& polygon
    ) const;

private:
    // width of the rendering output the camera draws into
    std::int32_t output_width = 0;

    // height of the rendering output the camera draws into
    std::int32_t output_height = 0;
};

}

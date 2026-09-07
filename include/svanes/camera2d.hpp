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

class Camera2D final {
public:
    // x axis position of the top-left corner of the camera in world coordinates
    float x = 0.0F;
    
    // y axis position of the top-left corner of the camera in world coordinates
    float y = 0.0F;

    // zoom factor of the camera, where 1.0 means no zoom,
    // > 1.0 means zoomed in, and < 1.0 means zoomed out
    float zoom = 1.0F;

    /**
     * Sets the zoom factor of the camera, keeping the specified screen coordinates
     * anchored to the same world coordinates.
     * 
     * @param new_zoom The new zoom factor to set. Must be finite and greater than zero.
     * @param screen_position The position in screen space to anchor.
     * @param scale The scale factor depending on screen size used for proportional scaling. Default is 1.0.
     * @param offset The correction distance to center the world display when the player's screen is not 16:9. Default is {0.0F, 0.0F}.
     * 
     * @throws std::invalid_argument if the new zoom factor is not finite or is less than or equal to zero.
     */
    void SetZoomAt(float new_zoom, Vector2D screen_position, float scale = 1.0F, Vector2D offset = {});

    /**
     * Converts a rectangle from world coordinates to screen coordinates.
     * 
     * @param world The rectangle in world coordinates.
     * @param scale The scale factor depending on screen size.
     * @param offset The correction distance to center the world display when the player's screen is not 16:9.
     * 
     * @return The rectangle in screen coordinates.
     */
    Rectangle2D WorldToScreen(Rectangle2D world, float scale = 1.0F, Vector2D offset = {}) const;

    /**
     * Converts a rectangle from screen coordinates to world coordinates.
     * 
     * @param screen The rectangle in screen coordinates.
     * @param scale The scale factor depending on screen size.
     * @param offset The correction distance to center the world display when the player's screen is not 16:9.
     * 
     * @return The rectangle in world coordinates.
     * 
     * @throws std::invalid_argument if the zoom factor is not finite or is less than or equal to zero.
     */
    Rectangle2D ScreenToWorld(Rectangle2D screen, float scale = 1.0F, Vector2D offset = {}) const;

    /**
     * Prepares a rectangular entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the rectangle is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param rectangle The Rectangle2D component of the entity to be rendered.
     * @param output_width The width of the rendering output.
     * @param output_height The height of the rendering output.
     * @param scale The scale factor depending on screen size.
     * @param offset The correction distance to center the world display when the player's screen is not 16:9.
     *
     * @return An optional Rectangle2D representing the destination rectangle in screen coordinates,
     * or std::nullopt if the rectangle is outside the bounds of the rendering output.
     *
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<Rectangle2D> PrepareForRendering(
        const Transform& transform, const Rectangle2D& rectangle,
        std::int32_t output_width, std::int32_t output_height, float scale, Vector2D offset
    ) const;

    /**
     * Prepares a triangular entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the triangle is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param triangle The Triangle2D component of the entity to be rendered.
     * @param output_width The width of the rendering output.
     * @param output_height The height of the rendering output.
     * @param scale The scale factor depending on screen size.
     * @param offset The correction distance to center the world display when the player's screen is not 16:9.
     * 
     * @return An optional Triangle2D representing the destination triangle in screen coordinates,
     * or std::nullopt if the triangle is outside the bounds of the rendering output.
     * 
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<Triangle2D> PrepareForRendering(
        const Transform& transform, const Triangle2D& triangle,
        std::int32_t output_width, std::int32_t output_height, float scale, Vector2D offset
    ) const;

    /**
     * Prepares a circular entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the circle is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param circle The Circle2D component of the entity to be rendered.
     * @param output_width The width of the rendering output.
     * @param output_height The height of the rendering output.
     * @param scale The scale factor depending on screen size.
     * @param offset The correction distance to center the world display when the player's screen is not 16:9.
     * 
     * @return An optional Circle2D representing the destination circle in screen coordinates,
     * or std::nullopt if the circle is outside the bounds of the rendering output.
     * 
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<Circle2D> PrepareForRendering(
        const Transform& transform, const Circle2D& circle,
        std::int32_t output_width, std::int32_t output_height, float scale, Vector2D offset
    ) const;

    /**
     * Prepares a convex polygon entity for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the polygon is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param polygon The ConvexPolygon2D component of the entity to be rendered.
     * @param output_width The width of the rendering output.
     * @param output_height The height of the rendering output.
     * @param scale The scale factor depending on screen size.
     * @param offset The correction distance to center the world display when the player's screen is not 16:9.
     * 
     * @return An optional ConvexPolygon2D representing the destination polygon in screen coordinates,
     * or std::nullopt if the polygon is outside the bounds of the rendering output.
     * 
     * @throws std::invalid_argument if the zoom factor or scale is not finite or is less than or equal to zero.
     */
    std::optional<ConvexPolygon2D> PrepareForRendering(
        const Transform& transform, const ConvexPolygon2D& polygon,
        std::int32_t output_width, std::int32_t output_height, float scale, Vector2D offset
    ) const;
};

}

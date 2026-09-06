#pragma once

#include <svanes/render/basic_render_types.hpp>
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
    Rectangle WorldToScreen(Rectangle world) const;

    /**
     * Converts a rectangle from screen coordinates to world coordinates.
     * 
     * @param screen The rectangle in screen coordinates.
     * 
     * @return The rectangle in world coordinates.
     * 
     * @throws std::invalid_argument if the zoom factor is not finite or is less than or equal to zero.
     */
    Rectangle ScreenToWorld(Rectangle screen) const;

    /**
     * Prepares a rectangle for rendering by converting its world coordinates to screen coordinates
     * and checking if it is within the bounds of the rendering output.
     * If the rectangle is outside the bounds, std::nullopt is returned.
     * 
     * @param transform The Transform component of the entity to be rendered.
     * @param output_width The width of the rendering output.
     * @param output_height The height of the rendering output.
     * 
     * @return An optional Rectangle representing the destination rectangle in screen coordinates,
     * or std::nullopt if the rectangle is outside the bounds of the rendering output.
     * 
     * @throws std::invalid_argument if the zoom factor is not finite or is less than or equal to zero.
     */
    std::optional<Rectangle> PrepareForRendering(
        const Transform& transform, std::int32_t output_width, std::int32_t output_height
    ) const;
};

}

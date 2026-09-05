#include <svanes/camera2d.hpp>

#include <svanes/render/render_system.hpp>
#include <svanes/rectangle_geometry.hpp>

#include <cmath>
#include <stdexcept>

namespace svanes {

/**
 * Validates that the zoom factor is finite and greater than zero.
 * If the zoom factor is invalid, an std::invalid_argument exception is thrown.
 * @param zoom The zoom factor to validate.
 * 
 * @throws std::invalid_argument if the zoom factor is not finite or is less than or equal to zero.
 */
static void ValidateZoom(float zoom)
{
    if (!std::isfinite(zoom) || zoom <= 0.0F) {
        throw std::invalid_argument("Camera zoom must be finite and greater than zero.");
    }
}

void Camera2D::SetZoomAt(float new_zoom, float screen_x, float screen_y)
{
    ValidateZoom(new_zoom);
    if (new_zoom == zoom) {
        return;
    }

    // We want to keep the world coordinates of the point at (screen_x, screen_y) 
    // the same before and after the zoom change.
    // ScreenToWorld of the same point before and after the zoom change 
    // should yield the same world coordinates.
    
    // So we need to update the top left corner of the camera (x, y)
    // to ensure that the world coordinates of the point at (screen_x, screen_y) remain unchanged.

    // Let (screen_x, screen_y) be the point in screen coordinates that we want to keep anchored.
    // Let (world_x, world_y) be the corresponding point in world coordinates before the zoom change.
    // Let (new_world_x, new_world_y) be the corresponding point in world coordinates after the zoom change.
    // We want (world_x, world_y) to be equal to (new_world_x, new_world_y).

    // Before the zoom change:
    // world_x = (screen_x / zoom) + x
    // world_y = (screen_y / zoom) + y

    // After the zoom change:
    // new_world_x = (screen_x / new_zoom) + new_x
    // new_world_y = (screen_y / new_zoom) + new_y

    // Solve for new_x and new_y:
    //    (screen_x / zoom) + x = (screen_x / new_zoom) + new_x
    // -> (screen_x / zoom) + x - (screen_x / new_zoom) = new_x
    // 
    //    (screen_y / zoom) + y = (screen_y / new_zoom) + new_y
    // -> (screen_y / zoom) + y - (screen_y / new_zoom) = new_y

    // Note that 
    // anchor.x = (screen_x / zoom) + x
    // anchor.y = (screen_y / zoom) + y

    // So
    // new_x = anchor.x - (screen_x / new_zoom)
    // new_y = anchor.y - (screen_y / new_zoom)

    const Rectangle anchor = ScreenToWorld({screen_x, screen_y, 0.0F, 0.0F});
    zoom = new_zoom;
    x = anchor.x - screen_x / zoom;
    y = anchor.y - screen_y / zoom;
}

Rectangle Camera2D::WorldToScreen(Rectangle world) const
{
    ValidateZoom(zoom);
    world.x = (world.x - x) * zoom;
    world.y = (world.y - y) * zoom;
    world.width *= zoom;
    world.height *= zoom;
    return world;
}

Rectangle Camera2D::ScreenToWorld(Rectangle screen) const
{
    ValidateZoom(zoom);
    screen.x = screen.x / zoom + x;
    screen.y = screen.y / zoom + y;
    screen.width /= zoom;
    screen.height /= zoom;
    return screen;
}

std::optional<Rectangle> Camera2D::PrepareForRendering(
    const Transform& transform, std::int32_t output_width, std::int32_t output_height
) const
{
    // If the rectangle is infinitely small, or the output is infinitely small, we can skip rendering it.
    if (output_width <= 0 || output_height <= 0 || transform.width <= 0.0F || transform.height <= 0.0F) {
        return std::nullopt;
    }

    const Rectangle destination = WorldToScreen({transform.x, transform.y, transform.width, transform.height});


    // Next, we need to check if the rectangle is within the bounds of the rendering output.

    const Rectangle bounds = RectangleGeometry(destination, transform.rotation).Bounds();

    // If it is not, we can again skip rendering it.

    if (bounds.x + bounds.width <= 0.0F || bounds.y + bounds.height <= 0.0F ||
        bounds.x >= static_cast<float>(output_width) || bounds.y >= static_cast<float>(output_height)) {
        return std::nullopt;
    }

    return destination;
}

}

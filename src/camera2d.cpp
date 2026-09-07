#include <svanes/camera2d.hpp>

#include <svanes/render/render_system.hpp>
#include <svanes/rectangle_geometry.hpp>
#include <svanes/triangle_geometry.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace svanes {

/**
 * Validates that a factor (zoom or scale) is finite and greater than zero.
 * If it is not, an std::invalid_argument exception is thrown.
 * @param factor The factor to validate.
 *
 * @throws std::invalid_argument if the factor is not finite or is less than or equal to zero.
 */
static void ValidateFactor(float factor)
{
    if (!std::isfinite(factor) || factor <= 0.0F) {
        throw std::invalid_argument("Camera factor must be finite and greater than zero.");
    }
}

/**
 * Checks if a rectangle is outside the bounds of the rendering output.
 * 
 * @param bounds The rectangle to check, in screen coordinates.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 * 
 * @return true if the rectangle is outside the bounds of the rendering output, false otherwise.
 */
static bool IsOutsideOutput(const Rectangle2D& bounds, std::int32_t output_width, std::int32_t output_height)
{
    return bounds.x + bounds.width * 0.5F <= 0.0F || bounds.y + bounds.height * 0.5F <= 0.0F ||
        bounds.x - bounds.width * 0.5F >= static_cast<float>(output_width) ||
        bounds.y - bounds.height * 0.5F >= static_cast<float>(output_height);
}

/**
 * Checks if a circle is outside the bounds of the rendering output.
 * 
 * @param circle The circle to check, in screen coordinates.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 * 
 * @return true if the circle is outside the bounds of the rendering output, false otherwise.
 */
static bool IsOutsideOutput(const Circle2D& circle, std::int32_t output_width, std::int32_t output_height)
{
    // Find the closest point on the rectangle defined by the output dimensions to the center of the circle.
    const float closest_x = std::clamp(circle.x, 0.0F, static_cast<float>(output_width));
    const float closest_y = std::clamp(circle.y, 0.0F, static_cast<float>(output_height));

    // If the distance from the circle's center to this closest point is greater than or equal to the radius,
    // then the circle is outside the bounds of the output.
    return std::hypot(circle.x - closest_x, circle.y - closest_y) >= circle.radius;
}

void Camera2D::SetZoomAt(float new_zoom, Vector2D screen_position)
{
    ValidateFactor(new_zoom);
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

    const Rectangle2D anchor = ScreenToWorld({screen_position.x, screen_position.y, 0.0F, 0.0F});
    zoom = new_zoom;
    x = anchor.x - screen_position.x / zoom;
    y = anchor.y - screen_position.y / zoom;
}

Rectangle2D Camera2D::WorldToScreen(Rectangle2D world) const
{
    ValidateFactor(zoom);
    world.x = (world.x - x) * zoom;
    world.y = (world.y - y) * zoom;
    world.width *= zoom;
    world.height *= zoom;
    return world;
}

Rectangle2D Camera2D::ScreenToWorld(Rectangle2D screen) const
{
    ValidateFactor(zoom);
    screen.x = screen.x / zoom + x;
    screen.y = screen.y / zoom + y;
    screen.width /= zoom;
    screen.height /= zoom;
    return screen;
}

std::optional<Circle2D> Camera2D::PrepareForRendering(
    const Transform& transform, const Circle2D& circle,
    std::int32_t output_width, std::int32_t output_height, float scale, Vector2D offset
) const
{
    ValidateFactor(scale);
    ValidateFactor(zoom);

    if (output_width <= 0 || output_height <= 0) {
        return std::nullopt;
    }

    Circle2D destination = TransformCircle(circle, transform);
    destination.x = (destination.x - x) * zoom * scale + offset.x;
    destination.y = (destination.y - y) * zoom * scale + offset.y;
    destination.radius *= zoom * scale;

    if (IsOutsideOutput(destination, output_width, output_height)) {
        return std::nullopt;
    }

    return destination;
}

std::optional<Triangle2D> Camera2D::PrepareForRendering(
    const Transform& transform, const Triangle2D& triangle,
    std::int32_t output_width, std::int32_t output_height, float scale, Vector2D offset
) const
{
    ValidateFactor(scale);
    ValidateFactor(zoom);

    if (output_width <= 0 || output_height <= 0) {
        return std::nullopt;
    }

    Triangle2D destination = TransformTriangle(triangle, transform);
    for (Vector2D& vertex : destination.vertices) {
        vertex.x = (vertex.x - x) * zoom * scale + offset.x;
        vertex.y = (vertex.y - y) * zoom * scale + offset.y;
    }

    if (IsOutsideOutput(TriangleGeometry(destination).Bounds(), output_width, output_height)) {
        return std::nullopt;
    }

    return destination;
}

std::optional<Rectangle2D> Camera2D::PrepareForRendering(
    const Transform& transform, const Rectangle2D& rectangle,
    std::int32_t output_width, std::int32_t output_height, float scale, Vector2D offset
) const
{
    ValidateFactor(scale);
    ValidateFactor(zoom);

    // If the rectangle is infinitely small, or the output is infinitely small, we can skip rendering it.
    if (output_width <= 0 || output_height <= 0 || rectangle.width <= 0.0F || rectangle.height <= 0.0F) {
        return std::nullopt;
    }

    Rectangle2D destination = WorldToScreen(TransformRectangle(rectangle, transform));
    destination.x *= scale;
    destination.y *= scale;
    destination.width *= scale;
    destination.height *= scale;
    destination.x += offset.x;
    destination.y += offset.y;


    // Next, we need to check if the rectangle is within the bounds of the rendering output.

    const Rectangle2D bounds = RectangleGeometry(destination, transform.rotation).Bounds();

    // If it is not, we can again skip rendering it.

    if (IsOutsideOutput(bounds, output_width, output_height)) {
        return std::nullopt;
    }

    return destination;
}

}

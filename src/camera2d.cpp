#include <svanes/camera2d.hpp>

#include <svanes/render/render_system.hpp>
#include <svanes/rectangle_geometry.hpp>

namespace svanes {

Rectangle Camera2D::WorldToScreen(Rectangle world) const
{
    world.x -= x;
    world.y -= y;
    return world;
}

Rectangle Camera2D::ScreenToWorld(Rectangle screen) const
{
    screen.x += x;
    screen.y += y;
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

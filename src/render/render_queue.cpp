/**
 * Implements the RenderQueue class, 
 * which is responsible for managing a queue
 * of rendering commands to be executed.
 */

#include <svanes/render/render_queue.hpp>

#include <algorithm>
#include <limits>

namespace svanes {

// Currently all the commands simply add the command

void RenderQueue::Clear(Color color)
{
    commands.emplace_back(ClearCommand{color});
}

void RenderQueue::DrawRectangle(Rectangle2D destination, Color color, float rotation, std::int32_t z_order)
{
    commands.emplace_back(RectangleCommand{destination, color, rotation, z_order});
}

void RenderQueue::DrawTriangle(Triangle2D destination, Color color, std::int32_t z_order)
{
    commands.emplace_back(TriangleCommand{destination, color, z_order});
}

void RenderQueue::DrawCircle(Circle2D destination, Color color, std::int32_t z_order)
{
    commands.emplace_back(CircleCommand{destination, color, z_order});
}

void RenderQueue::DrawConvexPolygon(const ConvexPolygon2D& destination, Color color, std::int32_t z_order)
{
    commands.emplace_back(ConvexPolygonCommand{destination, color, z_order});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle2D destination, float rotation, std::int32_t z_order)
{
    commands.emplace_back(TextureCommand{texture, std::nullopt, destination, rotation, z_order});
}

void RenderQueue::DrawTexture(
    TextureHandle texture, Rectangle2D source, Rectangle2D destination, float rotation, std::int32_t z_order
)
{
    commands.emplace_back(TextureCommand{texture, source, destination, rotation, z_order});
}

// Except for reset which just clears the command queue

void RenderQueue::Reset() noexcept
{
    commands.clear();
}

void RenderQueue::SortByZOrder()
{
    const auto sort_key = [](const Command& command) {
        if (std::get_if<ClearCommand>(&command) != nullptr) {
            return std::numeric_limits<std::int32_t>::min();
        }

        if (const auto* rectangle = std::get_if<RectangleCommand>(&command)) {
            return rectangle->z_order;
        }

        if (const auto* triangle = std::get_if<TriangleCommand>(&command)) {
            return triangle->z_order;
        }

        if (const auto* circle = std::get_if<CircleCommand>(&command)) {
            return circle->z_order;
        }

        if (const auto* polygon = std::get_if<ConvexPolygonCommand>(&command)) {
            return polygon->z_order;
        }

        return std::get<TextureCommand>(command).z_order;
    };

    std::stable_sort(
        commands.begin(), commands.end(),
        [&sort_key](const Command& left, const Command& right) {
            return sort_key(left) < sort_key(right);
        }
    );
}

}

/**
 * Implements the RenderQueue class,
 * which is responsible for managing a queue
 * of rendering commands to be executed.
 */

#include <svanes/render/render_queue.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace svanes {

// Currently all the commands simply add the command

void RenderQueue::Clear(Color color) { Add(ClearCommand{color}); }

void RenderQueue::DrawRectangle(Rectangle2D destination, Color color,
                                float rotation, std::int32_t z_order,
                                BlendMode blend_mode) {
    Add(RectangleCommand{destination, color, rotation, z_order, blend_mode});
}

void RenderQueue::DrawTriangle(Triangle2D destination, Color color,
                               std::int32_t z_order, BlendMode blend_mode) {
    Add(TriangleCommand{destination, color, z_order, blend_mode});
}

void RenderQueue::DrawCircle(Circle2D destination, Color color,
                             std::int32_t z_order, BlendMode blend_mode) {
    Add(CircleCommand{destination, color, color, z_order, blend_mode});
}

void RenderQueue::DrawRadialGradient(Circle2D destination, Color center_color,
                                     Color edge_color, std::int32_t z_order,
                                     BlendMode blend_mode) {
    Add(CircleCommand{destination, center_color, edge_color, z_order,
                      blend_mode});
}

void RenderQueue::DrawConvexPolygon(const ConvexPolygon2D &destination,
                                    Color color, std::int32_t z_order,
                                    BlendMode blend_mode) {
    Add(ConvexPolygonCommand{destination, color, z_order, blend_mode});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle2D destination,
                              float rotation, std::int32_t z_order,
                              BlendMode blend_mode, Color tint) {
    Add(TextureCommand{texture, std::nullopt, destination, rotation, z_order,
                       blend_mode, tint});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle2D source,
                              Rectangle2D destination, float rotation,
                              std::int32_t z_order, BlendMode blend_mode,
                              Color tint) {
    Add(TextureCommand{texture, source, destination, rotation, z_order,
                       blend_mode, tint});
}

// Except for reset which clears the command queue and the clip rectangle

void RenderQueue::Reset() noexcept {
    commands.clear();
    current_clip.reset();
}

void RenderQueue::SetClip(std::optional<Rectangle2D> clip) {
    current_clip = clip;
}

void RenderQueue::Add(Command command) {
    commands.push_back(
        QueuedCommand{.command = std::move(command), .clip = current_clip});
}

void RenderQueue::SortByZOrder() {
    const auto sort_key = [](const QueuedCommand &queued) {
        const Command &command = queued.command;
        if (std::get_if<ClearCommand>(&command) != nullptr) {
            return std::numeric_limits<std::int32_t>::min();
        }

        if (const auto *rectangle = std::get_if<RectangleCommand>(&command)) {
            return rectangle->z_order;
        }

        if (const auto *triangle = std::get_if<TriangleCommand>(&command)) {
            return triangle->z_order;
        }

        if (const auto *circle = std::get_if<CircleCommand>(&command)) {
            return circle->z_order;
        }

        if (const auto *polygon = std::get_if<ConvexPolygonCommand>(&command)) {
            return polygon->z_order;
        }

        return std::get<TextureCommand>(command).z_order;
    };

    std::stable_sort(
        commands.begin(), commands.end(),
        [&sort_key](const QueuedCommand &left, const QueuedCommand &right) {
            return sort_key(left) < sort_key(right);
        });
}

} // namespace svanes

#include <svanes/render/render_system.hpp>

#include <svanes/camera2d.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_queue.hpp>
#include <svanes/vector2d.hpp>

#include <algorithm>

namespace svanes {

// Reference width for scaling and centering the game world on different output sizes.
constexpr std::int32_t kDesignWidth = 1920;

// Reference height for scaling and centering the game world on different output sizes.
constexpr std::int32_t kDesignHeight = 1080;

/**
 * Computes the factor by which entity sizes and positions should be scaled
 * to fit the current output size, based on the given scale mode.
 *
 * @param mode The current scale mode.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 *
 * @return 1.0 under ScaleMode::Constant. Under ScaleMode::Proportional, the
 * ratio of output size to the design resolution on whichever axis is more
 * constrained, so scaling stays uniform on both axes.
 */
static float ComputeScale(ScaleMode mode, std::int32_t output_width, std::int32_t output_height)
{
    if (mode == ScaleMode::Constant) {
        return 1.0F;
    }

    const float width_ratio = static_cast<float>(output_width) / static_cast<float>(kDesignWidth);
    const float height_ratio = static_cast<float>(output_height) / static_cast<float>(kDesignHeight);
    return std::min(width_ratio, height_ratio);
}

/**
 * Computes the offset needed to center the scaled design resolution within
 * the current output size, so that under ScaleMode::Proportional the
 * leftover space on whichever axis isn't the constraining one is split
 * evenly on both sides, rather than left entirely on one side.
 *
 * @param mode The current scale mode.
 * @param scale The scale factor computed by ComputeScale.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 *
 * @return A zero offset under ScaleMode::Constant, or the centering offset under ScaleMode::Proportional.
 */
static Vector2D ComputeOffset(ScaleMode mode, float scale, std::int32_t output_width, std::int32_t output_height)
{
    if (mode == ScaleMode::Constant) {
        return {0.0F, 0.0F};
    }

    const float offset_x = (static_cast<float>(output_width) - static_cast<float>(kDesignWidth) * scale) * 0.5F;
    const float offset_y = (static_cast<float>(output_height) - static_cast<float>(kDesignHeight) * scale) * 0.5F;
    return {offset_x, offset_y};
}

void SubmitRectangles(
    const Registry& world, RenderQueue& render_queue, const Camera2D& camera,
    std::int32_t output_width, std::int32_t output_height, ScaleMode mode
)
{
    const float scale = ComputeScale(mode, output_width, output_height);
    const Vector2D offset = ComputeOffset(mode, scale, output_width, output_height);
    world.ForEach<Transform, Rectangle2D, SolidRectangle>(
        // Dont need the entity but the ForEach template will pass it in
        [&](Entity /*entity*/, const Transform& transform, const Rectangle2D& geometry, const SolidRectangle& rectangle) {
            const auto destination = camera.PrepareForRendering(transform, geometry, output_width, output_height, scale, offset);
            if (!destination) {
                return;
            }
            render_queue.DrawRectangle(
                *destination,
                rectangle.color,
                transform.rotation
            );
        }
    );
}

void SubmitSprites(
    const Registry& world, RenderQueue& render_queue, const Camera2D& camera,
    std::int32_t output_width, std::int32_t output_height, ScaleMode mode
)
{
    const float scale = ComputeScale(mode, output_width, output_height);
    const Vector2D offset = ComputeOffset(mode, scale, output_width, output_height);
    world.ForEach<Transform, Rectangle2D, Sprite>(
        // Again don't need the entity but the ForEach template will pass it in
        [&](Entity /*entity*/, const Transform& transform, const Rectangle2D& geometry, const Sprite& sprite) {
            const auto destination = camera.PrepareForRendering(transform, geometry, output_width, output_height, scale, offset);
            if (!destination) {
                return;
            }

            if (sprite.source.has_value()) {
                render_queue.DrawTexture(sprite.texture, *sprite.source, *destination, transform.rotation);
            } else {
                render_queue.DrawTexture(sprite.texture, *destination, transform.rotation);
            }
        }
    );
}

}

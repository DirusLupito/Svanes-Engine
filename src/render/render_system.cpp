#include <svanes/render/render_system.hpp>

#include <svanes/camera2d.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_queue.hpp>

namespace svanes {

void SubmitRectangles(
    const Registry& world, RenderQueue& render_queue, const Camera2D& camera,
    std::int32_t output_width, std::int32_t output_height
)
{
    world.ForEach<Transform, SolidRectangle>(
        // Dont need the entity but the ForEach template will pass it in
        [&](Entity /*entity*/, const Transform& transform, const SolidRectangle& rectangle) {
            const auto destination = camera.PrepareForRendering(transform, output_width, output_height);
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
    std::int32_t output_width, std::int32_t output_height
)
{
    world.ForEach<Transform, Sprite>(
        // Again don't need the entity but the ForEach template will pass it in
        [&](Entity /*entity*/, const Transform& transform, const Sprite& sprite) {
            const auto destination = camera.PrepareForRendering(transform, output_width, output_height);
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

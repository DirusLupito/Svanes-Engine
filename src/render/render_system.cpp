#include <svanes/render/render_system.hpp>

#include <svanes/registry.hpp>
#include <svanes/render/render_queue.hpp>

namespace svanes {

void SubmitRectangles(const Registry& world, RenderQueue& render_queue)
{
    world.ForEach<Transform, SolidRectangle>(
        // Dont need the entity but the ForEach template will pass it in
        [&render_queue](Entity /*entity*/, const Transform& transform, const SolidRectangle& rectangle) {
            render_queue.DrawRectangle(
                Rectangle{transform.x, transform.y, transform.width, transform.height},
                rectangle.color,
                transform.rotation
            );
        }
    );
}

void SubmitSprites(const Registry& world, RenderQueue& render_queue)
{
    world.ForEach<Transform, Sprite>(
        // Again don't need the entity but the ForEach template will pass it in
        [&render_queue](Entity /*entity*/, const Transform& transform, const Sprite& sprite) {
            const Rectangle destination{
                transform.x,
                transform.y,
                transform.width,
                transform.height,
            };

            if (sprite.source.has_value()) {
                render_queue.DrawTexture(sprite.texture, *sprite.source, destination, transform.rotation);
            } else {
                render_queue.DrawTexture(sprite.texture, destination, transform.rotation);
            }
        }
    );
}

}

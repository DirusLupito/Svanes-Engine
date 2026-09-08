#include <svanes/render/render_system.hpp>

#include <svanes/camera2d.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_queue.hpp>
#include <svanes/vector2d.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <variant>

namespace svanes {

/**
 * Reads the z order an entity is drawn at. The ZOrder component is optional,
 * so an entity without one is drawn at z order 0.
 *
 * @param world The registry containing all entities and their components.
 * @param entity The entity to read the z order of.
 *
 * @return The value of the entity's ZOrder component, or 0 if it has none.
 */
static std::int32_t ZOrderOf(const Registry& world, Entity entity)
{
    if (!world.HasComponent<ZOrder>(entity)) {
        return 0;
    }

    return world.GetComponent<ZOrder>(entity).value;
}

/**
 * Submits a rectangular entity to the render queue for rendering. The rectangle's world coordinates
 * are converted to screen coordinates using the camera, and if the rectangle is outside the bounds
 * of the rendering output, it is not submitted for rendering.
 * 
 * @param shape The Rectangle2D component of the entity to be rendered.
 * @param transform The Transform component of the entity to be rendered.
 * @param color The color to render the rectangle with.
 * @param z_order The z order to draw the rectangle at.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 */
static void SubmitShape(
    const Rectangle2D& shape, const Transform& transform, Color color, std::int32_t z_order,
    RenderQueue& render_queue, const Camera2D& camera
)
{
    const auto destination = camera.PrepareForRendering(transform, shape);

    if (!destination) {
        return;
    }

    render_queue.DrawRectangle(*destination, color, transform.rotation, z_order);
}

/**
 * Submits a triangular entity to the render queue for rendering. The triangle's world coordinates
 * are converted to screen coordinates using the camera, and if the triangle is outside the bounds
 * of the rendering output, it is not submitted for rendering.
 * 
 * @param shape The Triangle2D component of the entity to be rendered.
 * @param transform The Transform component of the entity to be rendered.
 * @param color The color to render the triangle with.
 * @param z_order The z order to draw the triangle at.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 */
static void SubmitShape(
    const Triangle2D& shape, const Transform& transform, Color color, std::int32_t z_order,
    RenderQueue& render_queue, const Camera2D& camera
)
{
    const auto destination = camera.PrepareForRendering(transform, shape);

    if (!destination) {
        return;
    }

    render_queue.DrawTriangle(*destination, color, z_order);
}

/**
 * Submits a circular entity to the render queue for rendering. The circle's world coordinates
 * are converted to screen coordinates using the camera, and if the circle is outside the bounds
 * of the rendering output, it is not submitted for rendering.
 * 
 * @param shape The Circle2D component of the entity to be rendered.
 * @param transform The Transform component of the entity to be rendered.
 * @param color The color to render the circle with.
 * @param z_order The z order to draw the circle at.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 */
static void SubmitShape(
    const Circle2D& shape, const Transform& transform, Color color, std::int32_t z_order,
    RenderQueue& render_queue, const Camera2D& camera
)
{
    const auto destination = camera.PrepareForRendering(transform, shape);

    if (!destination) {
        return;
    }

    render_queue.DrawCircle(*destination, color, z_order);
}

/**
 * Submits a convex polygonal entity to the render queue for rendering. The polygon's world coordinates
 * are converted to screen coordinates using the camera, and if the polygon is outside the bounds
 * of the rendering output, it is not submitted for rendering.
 * 
 * @param shape The ConvexPolygon2D component of the entity to be rendered.
 * @param transform The Transform component of the entity to be rendered.
 * @param color The color to render the polygon with.
 * @param z_order The z order to draw the polygon at.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 */
static void SubmitShape(
    const ConvexPolygon2D& shape, const Transform& transform, Color color, std::int32_t z_order,
    RenderQueue& render_queue, const Camera2D& camera
)
{
    const auto destination = camera.PrepareForRendering(transform, shape);
    if (!destination) {
        return;
    }
    render_queue.DrawConvexPolygon(*destination, color, z_order);
}

/**
 * Recursively submits all parts of a composite shape to the render queue for rendering.
 * Each part's transform is composed with the parent transform to determine its final position and rotation.
 * 
 * @param shape The composite shape to be submitted for rendering.
 * @param transform The transform of the parent entity, which will be combined with each part's transform.
 * @param color The color to render the composite shape with.
 * @param z_order The z order to draw the composite shape at.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 */
static void SubmitShape(
    const CompositeShape2D& shape, const Transform& transform, Color color, std::int32_t z_order,
    RenderQueue& render_queue, const Camera2D& camera
)
{
    for (const GeometryPart2D& part : shape.parts) {
        const Transform pose = ComposeTransforms(transform, part.transform);

        std::visit([&](const auto& primitive) {
            SubmitShape(primitive, pose, color, z_order, render_queue, camera);
        }, part.shape);
    }
}

void SubmitShapes(const Registry& world, RenderQueue& render_queue, const Camera2D& camera)
{
    world.ForEach<Transform, SolidShape>(
        [&](Entity entity, const Transform& transform, const SolidShape& visual) {
            const std::int32_t z_order = ZOrderOf(world, entity);

            // std::visit calls a function with whichever value a std::variant currently holds
            // The visitor function must handle all possible types contained within the variant.
            // If a type is unhandled, the code will fail to compile.
            std::visit([&](const auto& shape) {
                SubmitShape(shape, transform, visual.color, z_order, render_queue, camera);
            }, visual.geometry);
        }
    );
}

void SubmitSprites(const Registry& world, RenderQueue& render_queue, const Camera2D& camera)
{
    world.ForEach<Transform, Sprite>(
        [&](Entity entity, const Transform& transform, const Sprite& sprite) {
            const auto destination = camera.PrepareForRendering(transform, sprite.geometry);
            if (!destination) {
                return;
            }

            const std::int32_t z_order = ZOrderOf(world, entity);
            if (sprite.source.has_value()) {
                render_queue.DrawTexture(sprite.texture, *sprite.source, *destination, transform.rotation, z_order);
            } else {
                render_queue.DrawTexture(sprite.texture, *destination, transform.rotation, z_order);
            }
        }
    );
}

}

#pragma once

#include <svanes/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <optional>

namespace svanes {

class Camera2D;
class Registry;
class RenderQueue;

/**
 * Represents a sprite component that can be attached to an entity for rendering.
 * This component holds a reference to a texture and an optional source rectangle that defines
 * which part of the texture to render. If the source rectangle is not provided, 
 * the entire texture will be rendered. The geometry field defines the rectangle
 * on the screen where the sprite will be drawn.
 * 
 * FIELDS:
 * - texture: A handle to the texture resource to be rendered.
 * - source: An optional rectangle defining the portion of the texture to render. 
 * If not provided, the entire texture will be used.
 * - geometry: The rectangle onto which the sprite source will be drawn.
 */
struct Sprite {
    TextureHandle texture;
    std::optional<Rectangle2D> source;
    Rectangle2D geometry;
};

/**
 * Represents both the color and geometry of an entity to be rendered. 
 * This component is used to define simple solid shapes that can be drawn
 * directly to the screen without the need for a texture.
 *
 * FIELDS:
 * - color: The color value used to fill the entity's geometry.
 * - geometry: The geometric shape of the entity to be rendered.
 */
struct SolidShape {
    Color color;
    Geometry2D geometry;
};

/**
 * Optional render ordering component. Entities are drawn in ascending order of
 * their z order, so an entity with a lower z order appears behind an entity with
 * a higher one, regardless of which geometry either of them uses.
 *
 * This component may be omitted, in which case the entity is drawn as though its
 * z order were 0. Entities sharing the same z order are drawn in creation order.
 *
 * FIELDS:
 * - value: The z order the entity is drawn at. Lower values are drawn first.
 */
struct ZOrder {
    std::int32_t value = 0;
};

/**
 * Submits all entities with Transform and SolidShape components to the render queue for rendering.
 * 
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 */
void SubmitShapes(const Registry& world, RenderQueue& render_queue, const Camera2D& camera);

/**
 * Submits all entities with Transform and Sprite components to the render queue for rendering.
 * 
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 */
void SubmitSprites(const Registry& world, RenderQueue& render_queue, const Camera2D& camera);

}

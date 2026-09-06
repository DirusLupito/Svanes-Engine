#pragma once

#include <svanes/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <cstdint>
#include <optional>

namespace svanes {

class Camera2D;
class Registry;
class RenderQueue;

/**
 * Controls how entity sizes and positions are interpreted relative to the
 * current window size.
 *
 * MEMBERS:
 * - Constant: Pixel values are used verbatim, regardless of window size.
 * - Proportional: Pixel values are rescaled so their proportion of the
 *   screen stays constant across window sizes.
 */
enum class ScaleMode : std::uint8_t {
    Constant,
    Proportional,
};

/**
 * Represents the position and rotation of an entity's local origin in 2D space.
 * This component is used to determine where to place the entity's geometry in the world.
 * 
 * FIELDS:
 * - x: The x-coordinate of the entity's position.
 * - y: The y-coordinate of the entity's position.
 * - rotation: The rotation in radians.
 */
struct Transform {
    float x = 0.0F;
    float y = 0.0F;
    float rotation = 0.0F;
};

/**
 * Represents a sprite component that can be attached to an entity for rendering.
 * This component holds a reference to a texture and an optional source rectangle that defines
 * which part of the texture to render. If the source rectangle is not provided, 
 * the entire texture will be rendered.
 * 
 * FIELDS:
 * - texture: A handle to the texture resource to be rendered.
 * - source: An optional rectangle defining the portion of the texture to render. 
 * If not provided, the entire texture will be used.
 */
struct Sprite {
    TextureHandle texture;
    std::optional<Rectangle2D> source;
};

/**
 * Represents a solid rectangle component that can be attached to an entity for rendering.
 * This component holds a color that defines the fill color of the rectangle.
 * Note that the rectangle's position is determined by the Transform component of the entity,
 * while the size is determined by the Rectangle2D component of the entity.
 * 
 * FIELDS:
 * - color: The color of the rectangle to be rendered.
 */
struct SolidRectangle {
    Color color;
};

/**
 * Submits all entities with Transform, Rectangle2D and SolidRectangle components to the render queue for rendering.
 * 
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 * @param mode Controls how entity sizes and positions are scaled relative to the current window size.
 */
void SubmitRectangles(
    const Registry& world, RenderQueue& render_queue, const Camera2D& camera,
    std::int32_t output_width, std::int32_t output_height, ScaleMode mode
);

/**
 * Submits all entities with Transform, Rectangle2D and Sprite components to the render queue for rendering.
 * 
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 * @param mode Controls how entity sizes and positions are scaled relative to the current window size.
 */
void SubmitSprites(
    const Registry& world, RenderQueue& render_queue, const Camera2D& camera,
    std::int32_t output_width, std::int32_t output_height, ScaleMode mode
);

}

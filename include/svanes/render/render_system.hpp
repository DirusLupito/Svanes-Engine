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
 * Represents the layout of the rendering output, including the scale factor,
 * the offset to center the world display, and the viewport rectangle in screen coordinates.
 * 
 * FIELDS:
 * - scale: The factor by which entity sizes and positions should be scaled to fit the current output size.
 * - offset: The offset to center the scaled design resolution within the current output size.
 * - viewport: The rectangle in screen coordinates that defines the area of the output where rendering occurs.
 */
struct RenderLayout {
    float scale;
    Vector2D offset;
    Rectangle2D viewport;
};

/**
 * Computes the render layout based on the specified scale mode and output dimensions.
 * This will take in the width and height of the application window as well as the current
 * scale mode, and return a RenderLayout struct containing the computed scale to apply to
 * entity sizes and positions, the offset to center the world display, and the viewport rectangle
 * in screen coordinates. This tells the rendering system how to scale and position entities
 * appropriately for both constant and proportional scaling modes.
 *
 * @param mode The scale mode to use for computing the layout.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 * 
 * @return A RenderLayout struct containing the computed scale, offset, and viewport.
 */
RenderLayout ComputeRenderLayout(ScaleMode mode, std::int32_t output_width, std::int32_t output_height);

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

Vector2D ScreenToWorldPoint(
    const Camera2D& camera, Vector2D screen_point,
    std::int32_t output_width, std::int32_t output_height, ScaleMode mode
);

/**
 * Submits all entities with Transform and SolidShape components to the render queue for rendering.
 * 
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 * @param camera The camera used to convert world coordinates to screen coordinates.
 * @param output_width The width of the rendering output.
 * @param output_height The height of the rendering output.
 * @param mode Controls how entity sizes and positions are scaled relative to the current window size.
 */
void SubmitShapes(
    const Registry& world, RenderQueue& render_queue, const Camera2D& camera,
    std::int32_t output_width, std::int32_t output_height, ScaleMode mode
);

/**
 * Submits all entities with Transform and Sprite components to the render queue for rendering.
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

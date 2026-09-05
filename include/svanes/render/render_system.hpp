#pragma once

#include <svanes/render/basic_render_types.hpp>

#include <optional>

namespace svanes {

class Registry;
class RenderQueue;

/**
 * Represents the transformation and size of an entity in 2D space.
 * This component is used to determine where and how large an entity should be rendered.
 * 
 * FIELDS:
 * - x: The x-coordinate of the entity's position.
 * - y: The y-coordinate of the entity's position.
 * - width: The width of the entity.
 * - height: The height of the entity.
 * - rotation: The rotation in radians.
 */
struct Transform {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
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
    std::optional<Rectangle> source;
};

/**
 * Represents a solid rectangle component that can be attached to an entity for rendering.
 * This component holds a color that defines the fill color of the rectangle.
 * Note that the rectangle's position and size are determined by the Transform component of the entity.
 * 
 * FIELDS:
 * - color: The color of the rectangle to be rendered.
 */
struct SolidRectangle {
    Color color;
};

/**
 * Submits all entities with a Transform and SolidRectangle component to the render queue for rendering.
 * 
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 */
void SubmitRectangles(const Registry& world, RenderQueue& render_queue);

/**
 * Submits all entities with a Transform and Sprite component to the render queue for rendering.
 * 
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be submitted.
 */
void SubmitSprites(const Registry& world, RenderQueue& render_queue);

}

#pragma once

#include <svanes/geometry/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace svanes {

class Camera2D;
class Registry;
class RenderQueue;

/**
 * Represents a radial gradient to be drawn on the screen.
 * Essentially, this is a circle with a color that transitions
 * from a center color to an edge color, creating a gradient effect.
 *
 * FIELDS:
 * - geometry: The circle defining the area of the gradient.
 * - center_color: The color at the center of the gradient.
 * - edge_color: The color at the edge of the gradient.
 * - blend_mode: The blending mode used to combine the gradient with the
 * existing screen color. Defaults to BlendMode::Alpha.
 */
struct RadialGradient2D {
    Circle2D geometry;
    Color center_color;
    Color edge_color;
    BlendMode blend_mode = BlendMode::Alpha;
};

/**
 * Represents a sprite component that can be attached to an entity for
 * rendering. This component holds a reference to a texture and an optional
 * source rectangle that defines which part of the texture to render. If the
 * source rectangle is not provided, the entire texture will be rendered. The
 * geometry field defines the rectangle on the screen where the sprite will be
 * drawn.
 *
 * FIELDS:
 * - texture: A handle to the texture resource to be rendered.
 * - source: An optional rectangle defining the portion of the texture to
 * render. If not provided, the entire texture will be used.
 * - geometry: The rectangle onto which the sprite source will be drawn.
 * - blend_mode: The blending mode used to combine the sprite with the existing
 * screen color.
 */
struct Sprite {
    TextureHandle texture;
    std::optional<Rectangle2D> source;
    Rectangle2D geometry;
    BlendMode blend_mode = BlendMode::Alpha;
};

using TileId = std::uint16_t;

/**
 * Represents a single cell within a tile map. Each cell in a tile map holds an ID
 * that maps it to a tile in a tilesheet to render, as well as a flag marking whether
 * sollision should be enabled or not on this tile. A TileId of 0 is associated with
 * an empty cell.
 * 
 * FIELDS:
 * - tile_id: The ID that corresponds to a tile texture on a tilesheet for rendering
 * - collidable: true if this tile should have a collider, otherwise false
 */
struct TileMapCell {
    TileId tile_id = 0;
    bool collidable = false;
};

/**
 * Represents an array of tiles. Each tilemap should have a tileset, or atlas, from
 * which it can get textures to render for each tile. The dimensions of both the
 * tilemap and the tileset must be provided by the user for tiles to render properly.
 * Adjacent collider tiles are merged into larger rectangles to reduce the number of 
 * collision checks that are performed later.
 * 
 * FIELDS:
 * - atlas: The image containing the tileset
 * - columns: The number of columns in this tilemap
 * - rows: The number of rows in this tilemap
 * - tile_width: The width of tiles in this tilemap when rendered to the world
 * - tile_height: The height of tiles in this tilemap when rendered to the world
 * - atlas_tile_width: The width of tiles in the tileset
 * - atlas_tile_height: The height of tiles in the tileset
 * - atlas_columns: The number of columns in the tileset
 * - atlas_rows: The number of rows in the tileset
 * - blend_mode: Setting for how the tileamap blends with the background
 * - cells: Array containing all tiles in the tilemap
 * - collision_rectangles: Merged rectangles covering adjacent collidable tiles
 * - collision_cache_built: Flag indicating whether collision rectangles have been
 * populated or not.
 */
struct TileMap {
    TextureHandle atlas;
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    std::uint32_t tile_width = 0;
    std::uint32_t tile_height = 0;
    std::uint32_t atlas_tile_width = 0;
    std::uint32_t atlas_tile_height = 0;
    std::uint32_t atlas_columns = 0;
    std::uint32_t atlas_rows = 0;
    BlendMode blend_mode = BlendMode::Alpha;
    std::vector<TileMapCell> cells;
    std::vector<Rectangle2D> collision_rectangles;
    bool collision_cache_built = false;
};

/**
 * Represents both the color and geometry of an entity to be rendered.
 * This component is used to define simple solid shapes that can be drawn
 * directly to the screen without the need for a texture.
 *
 * FIELDS:
 * - color: The color value used to fill the entity's geometry.
 * - geometry: The geometric shape of the entity to be rendered.
 * - blend_mode: The blending mode used to combine the shape with the existing
 * screen color.
 */
struct SolidShape {
    Color color;
    Geometry2D geometry;
    BlendMode blend_mode = BlendMode::Alpha;
};

/**
 * Optional render ordering component. Entities are drawn in ascending order of
 * their z order, so an entity with a lower z order appears behind an entity
 * with a higher one, regardless of which geometry either of them uses.
 *
 * This component may be omitted, in which case the entity is drawn as though
 * its z order were 0. Entities sharing the same z order are drawn in creation
 * order.
 *
 * FIELDS:
 * - value: The z order the entity is drawn at. Lower values are drawn first.
 */
struct ZOrder {
    std::int32_t value = 0;
};

/**
 * Submits all entities with Transform and SolidShape components to the render
 * queue for rendering.
 *
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be
 * submitted.
 * @param camera The camera used to convert world coordinates to screen
 * coordinates.
 */
void SubmitShapes(const Registry &world, RenderQueue &render_queue,
                  const Camera2D &camera);

/**
 * Submits all entities with Transform and RadialGradient2D components to the
 * render queue for rendering.
 *
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be
 * submitted.
 * @param camera The camera used to convert world coordinates to screen
 * coordinates.
 */
void SubmitRadialGradients(const Registry &world, RenderQueue &render_queue,
                           const Camera2D &camera);

/**
 * Submits all entities with Transform and Sprite components to the render queue
 * for rendering.
 *
 * @param world The registry containing all entities and their components.
 * @param render_queue The render queue to which the rendering commands will be
 * submitted.
 * @param camera The camera used to convert world coordinates to screen
 * coordinates.
 */
void SubmitSprites(const Registry &world, RenderQueue &render_queue,
                   const Camera2D &camera);

/**
 * Submits all entities with Transform and TileMap components to the render queue
 * for rendering.
 *
 * @param world The registry containing all tilemaps.
 * @param render_queue The render queue to which the rendering commands will be
 * submitted.
 * @param camera The camera used to convert world coordinates to screen
 * coordinates.
 */
void SubmitTileMaps(const Registry &world, RenderQueue &render_queue,
                    const Camera2D &camera);

} // namespace svanes

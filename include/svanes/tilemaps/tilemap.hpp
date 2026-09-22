#pragma once

#include <svanes/collision_system.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <cstdint>
#include <vector>

namespace svanes {

class Camera2D;
class Registry;
class RenderQueue;

using TileId = std::uint16_t;

/**
 * Represents a single cell within a tile map. Each cell in a tile map holds an
 * ID that maps it to a tile in a tilesheet to render, as well as a flag marking
 * whether sollision should be enabled or not on this tile. A TileId of 0 is
 * associated with an empty cell.
 *
 * FIELDS:
 * - tile_id: The ID that corresponds to a tile texture on a tilesheet for
 * rendering
 * - collidable: true if this tile should have a collider, otherwise false
 */
struct TileMapCell {
    TileId tile_id = 0;
    bool collidable = false;
};

/**
 * Represents an array of tiles. Each tilemap should have a tileset, or atlas,
 * from which it can get textures to render for each tile. The dimensions of
 * both the tilemap and the tileset must be provided by the user for tiles to
 * render properly. Adjacent collider tiles are merged into larger rectangles to
 * reduce the number of collision checks that are performed later.
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
 * - collision_cache_built: Flag indicating whether collision rectangles have
 * been populated or not.
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
 * Combines adjacent collidable tiles into larger rectangles in order to reduce
 * the total number of collision objects to check later on. Scans the tilemap
 * row by row in order to find large areas that can be combined into larger
 * rectangles.
 *
 * @param tile_map The tilemap having its collision cache constructed
 */
void BuildTileMapCollisionCache(TileMap &tile_map);

/**
 * Detects all collisions between a tilemap and another generic 2D geometry.
 *
 * @param geometry The entity geometry to test for collisions.
 * @param transform The transform to apply to the geometry.
 * @param tile_map The tilemap to test for collisions.
 * @param tile_map_transform The transform of the tile map.
 *
 * @return A vector of Collision2D objects representing the detected collisions.
 * If no collisions are detected, the vector will be empty.
 */
std::vector<Collision2D>
DetectTileMapCollisions(const Geometry2D &geometry, const Transform &transform,
                        const TileMap &tile_map,
                        const Transform &tile_map_transform);

/**
 * Submits all entities with Transform and TileMap components to the render
 * queue for rendering.
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

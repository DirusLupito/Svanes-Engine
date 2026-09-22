#include <svanes/tilemaps/tilemap.hpp>

#include <cstddef>
#include <stdexcept>

namespace svanes {

std::vector<Collision2D>
DetectTileMapCollisions(const Geometry2D &geometry, const Transform &transform,
                        const TileMap &tile_map,
                        const Transform &tile_map_transform) {
    if (tile_map.columns == 0 || tile_map.rows == 0 ||
        tile_map.tile_width == 0 || tile_map.tile_height == 0) {
        throw std::invalid_argument(
            "TileMap collision dimensions must be positive.");
    }

    const std::uint64_t expected_cell_count =
        static_cast<std::uint64_t>(tile_map.columns) * tile_map.rows;
    if (expected_cell_count != tile_map.cells.size()) {
        throw std::invalid_argument(
            "TileMap cell count must match its row and column counts.");
    }

    std::vector<Collision2D> collisions;
    if (tile_map.collision_cache_built) {
        for (const Rectangle2D &tile_geometry : tile_map.collision_rectangles) {
            std::vector<Collision2D> tile_collisions =
                DetectCollisions(geometry, transform, Geometry2D{tile_geometry},
                                 tile_map_transform);
            collisions.insert(collisions.end(), tile_collisions.begin(),
                              tile_collisions.end());
        }
        return collisions;
    }

    for (std::uint32_t row = 0; row < tile_map.rows; ++row) {
        for (std::uint32_t column = 0; column < tile_map.columns; ++column) {
            const TileMapCell &cell =
                tile_map
                    .cells[static_cast<std::size_t>(row) * tile_map.columns +
                           column];
            if (cell.tile_id == 0 || !cell.collidable) {
                continue;
            }

            const Rectangle2D tile_geometry{
                (static_cast<float>(column) + 0.5F) *
                    static_cast<float>(tile_map.tile_width),
                (static_cast<float>(row) + 0.5F) *
                    static_cast<float>(tile_map.tile_height),
                static_cast<float>(tile_map.tile_width),
                static_cast<float>(tile_map.tile_height),
            };
            std::vector<Collision2D> tile_collisions =
                DetectCollisions(geometry, transform, Geometry2D{tile_geometry},
                                 tile_map_transform);
            collisions.insert(collisions.end(), tile_collisions.begin(),
                              tile_collisions.end());
        }
    }
    return collisions;
}

void BuildTileMapCollisionCache(TileMap &tile_map) {
    if (tile_map.columns == 0 || tile_map.rows == 0 ||
        tile_map.tile_width == 0 || tile_map.tile_height == 0) {
        throw std::invalid_argument(
            "TileMap collision dimensions must be positive.");
    }

    const std::uint64_t expected_cell_count =
        static_cast<std::uint64_t>(tile_map.columns) * tile_map.rows;
    if (expected_cell_count != tile_map.cells.size()) {
        throw std::invalid_argument(
            "TileMap cell count must match its row and column counts.");
    }

    tile_map.collision_rectangles.clear();
    tile_map.collision_cache_built = false;
    std::vector<bool> visited(tile_map.cells.size(), false);
    const float tile_width = static_cast<float>(tile_map.tile_width);
    const float tile_height = static_cast<float>(tile_map.tile_height);

    // Loop through all cells to find larger shapes
    // Scans row by row attempting to create larger rectangles
    // Not ideal, but good enough for now. If performance with
    // tilemaps becomes an issue this may be revisited.
    for (std::uint32_t row = 0; row < tile_map.rows; ++row) {
        for (std::uint32_t column = 0; column < tile_map.columns; ++column) {
            const std::size_t index =
                static_cast<std::size_t>(row) * tile_map.columns + column;
            const TileMapCell &cell = tile_map.cells[index];
            if (visited[index] || cell.tile_id == 0 || !cell.collidable) {
                continue;
            }

            std::uint32_t width = 1;
            while (column + width < tile_map.columns) {
                const std::size_t next_index =
                    static_cast<std::size_t>(row) * tile_map.columns + column +
                    width;
                const TileMapCell &next_cell = tile_map.cells[next_index];
                if (visited[next_index] || next_cell.tile_id == 0 ||
                    !next_cell.collidable) {
                    break;
                }
                ++width;
            }

            std::uint32_t height = 1;
            while (row + height < tile_map.rows) {
                bool row_is_solid = true;
                for (std::uint32_t offset = 0; offset < width; ++offset) {
                    const std::size_t next_index =
                        static_cast<std::size_t>(row + height) *
                            tile_map.columns +
                        column + offset;
                    const TileMapCell &next_cell = tile_map.cells[next_index];
                    if (visited[next_index] || next_cell.tile_id == 0 ||
                        !next_cell.collidable) {
                        row_is_solid = false;
                        break;
                    }
                }
                if (!row_is_solid) {
                    break;
                }
                ++height;
            }

            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    visited[static_cast<std::size_t>(row + y) *
                                tile_map.columns +
                            column + x] = true;
                }
            }

            // Add the new larger rectangle to the collision cache
            tile_map.collision_rectangles.push_back(Rectangle2D{
                (static_cast<float>(column) +
                 static_cast<float>(width) * 0.5F) *
                    tile_width,
                (static_cast<float>(row) + static_cast<float>(height) * 0.5F) *
                    tile_height,
                static_cast<float>(width) * tile_width,
                static_cast<float>(height) * tile_height,
            });
        }
    }
    tile_map.collision_cache_built = true;
}

} // namespace svanes

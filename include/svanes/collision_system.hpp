#pragma once

#include <svanes/geometry/geometry.hpp>

#include <svanes/vector2d.hpp>

#include <vector>

namespace svanes {

struct TileMap;

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
 * Component for 2D entity collision detection.
 *
 * FIELDS:
 * - geometry: The geometric shape of the entity used for collision detection.
 */
struct Collider2D {
    Geometry2D geometry;
};

/**
 * Structure describing the result of a collision detection between two 2D
 * shapes. Represents the direction needed to minimally separate the first shape
 * from the second, and the distance along that direction to reach a
 * non-penetrating state.
 *
 * FIELDS:
 * - normal: Unit direction in which to move the first shape out of the second.
 * - penetration_depth: World-space distance along normal needed to reach
 * touching. Zero means the shapes already touch without penetrating.
 */
struct Collision2D {
    Vector2D normal;
    float penetration_depth = 0.0F;
};

/**
 * Detects all collisions between two generic 2D geometries, which may be
 * primitive shapes or composite shapes. The function uses the Separating Axis
 * Theorem to determine if the shapes intersect and calculates the penetration
 * depth and normal of the collision if they do.
 *
 * @param a The first geometry to test for collisions.
 * @param transform_a The transform to apply to the first geometry.
 * @param b The second geometry to test for collisions.
 * @param transform_b The transform to apply to the second geometry.
 *
 * @return A vector of Collision2D objects representing the detected collisions.
 * If no collisions are detected, the vector will be empty.
 */
std::vector<Collision2D> DetectCollisions(const Geometry2D &a,
                                          const Transform &transform_a,
                                          const Geometry2D &b,
                                          const Transform &transform_b);

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

} // namespace svanes

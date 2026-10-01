#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace svanes {

class AsyncParallelForDriver;

/**
 * Represents an entry in a 2D hierarchical grid (HGrid) spatial partitioning
 * structure. Each entry consists of an entity and its associated axis-aligned
 * bounding box (AABB).
 *
 * FIELDS:
 * - entity: The unique identifier of the entity associated with this entry.
 * - bounds: The axis-aligned bounding box (AABB) of the entity in world
 * coordinates.
 */
struct HGridEntry2D {
    Entity entity;
    Rectangle2D bounds;
};

/**
 * A 2D hierarchical grid (HGrid) spatial partitioning structure for efficient
 * spatial queries. The HGrid divides the 2D space into a hierarchy of grid
 * cells, allowing for efficient querying of entities based on their
 * axis-aligned bounding boxes (AABBs).
 */
class HGrid2D {
public:
    /**
     * Given a set of entries, rebuilds the hierarchical grid structure to
     * accommodate them. This function clears any existing entries and
     * constructs the grid based on the provided entries.
     *
     * We choose the smallest cell size that is at least as large as both
     * dimensions of an object's AABB. For example, a rectangle five units wide
     * and two units tall belongs to level 3 (2^3 = 8 units). A rectangle
     * exactly eight units wide also fits there. One slightly wider than eight
     * belongs to level 4 (2^4 = 16 units).
     *
     * An object appears at only one level, in only one cell. Also, those
     * rectangles whose width and height are less than 2^0 = 1 unit are placed
     * in level 0.
     *
     * @param entries A span of HGridEntry2D objects representing the entities
     * and their bounding boxes to be added to the grid.
     *
     * @throws std::invalid_argument if any entity IDs are not unique or if any
     * entry's dimensions exceed 2^64.
     * @throws std::overflow_error if any entry's cell coordinates exceed the
     * range of int64_t.
     */
    void Rebuild(std::span<const HGridEntry2D> entries);

    /**
     * Queries the grid for all entities whose bounding boxes intersect with the
     * specified query rectangle.
     *
     * @param bounds The axis-aligned bounding box (AABB) to query.
     * @return A vector of entity IDs that intersect with the query bounds.
     */
    std::vector<Entity> Query(const Rectangle2D &bounds) const;

    /**
     * Broad phase collision detection.
     *
     * Builds a list of all unique pairs of entities that have overlapping
     * bounding boxes in the grid. Each pair is represented as a std::pair of
     * entity IDs. Every pair puts the smaller entity ID first. The order of
     * the pairs in the returned vector is unspecified. The grid must remain
     * unchanged until this call returns.
     *
     * @param driver The driver used to distribute entry searches.
     * @param batch_size The maximum number of entries per parallel work batch.
     * Must be positive. Defaults to 16.
     * @return A vector of unique pairs of entity IDs that have overlapping
     * bounding boxes.
     */
    std::vector<std::pair<Entity, Entity>>
    BuildCollisionPairs(AsyncParallelForDriver &driver,
                        std::size_t batch_size = 16) const;

private:
    /**
     * Represents a cell in the hierarchical grid. Each cell is identified by
     * its x and y coordinates in the grid.
     *
     * These cells may be assigned to different levels of the hierarchy.
     *
     * At level L of the hierarchy, each square has side length 2^L.
     * Cell (x,y) has its lower-left corner at (x*2^L, y*2^L).
     *
     * FIELDS:
     * - x: The x index of the cell in the grid.
     * - y: The y index of the cell in the grid.
     * - operator==: Compares two Cell objects for equality based on their
     * coordinates.
     */
    struct Cell {
        std::int64_t x;
        std::int64_t y;

        /**
         * Compares two Cell objects for equality based on their x and y
         * coordinates. They are considered equal if both their x and y
         * coordinates are the same.
         *
         * @return true if the cells are equal, false otherwise.
         */
        bool operator==(const Cell &) const = default;
    };

    /**
     * Hash type for Cell objects, allowing them to be used as keys in unordered
     * containers.
     *
     * FIELDS:
     * - operator(): Computes a hash value for a given Cell object based on its
     * coordinates.
     */
    struct CellHash {

        /**
         * Hashes a Cell object using its x and y coordinates as an
         * array of 4 32-bit words, which are then hashed by Hash.
         *
         * @param cell The Cell object to be hashed.
         *
         * @return A size_t hash value for the Cell object.
         */
        std::size_t operator()(const Cell &cell) const;
    };

    // Represents a level in the hierarchical grid, which is a mapping from Cell
    // objects to vectors of indices of entries that occupy those cells.
    using Level = std::unordered_map<Cell, std::vector<std::size_t>, CellHash>;

    /**
     * Visits all cells in the specified level of the grid that intersect with
     * the given query rectangle. For each cell, the provided visitor function
     * is called with the indices of entries that occupy that cell.
     *
     * @tparam Visitor The type of the visitor function to call for each cell.
     * @param level_index The index of the level to visit.
     * @param bounds The axis-aligned bounding box (AABB) to query.
     * @param visit A visitor function that takes a const reference to a vector
     * of indices of entries in the visited cell, and performs some operation on
     * them.
     */
    template <typename Visitor>
    void VisitCells(std::size_t level_index, const Rectangle2D &bounds,
                    const Visitor &visit) const;

    // Represents the entries in the hierarchical grid, where each entry
    // consists of an entity and its associated axis-aligned bounding box
    // (AABB). These entities may be assigned to different levels of the
    // hierarchy based on their dimensions.
    std::vector<HGridEntry2D> entries;

    // An array of 65 levels, where each level corresponds to a different cell
    // size in the hierarchy. Enough levels are provided to accommodate cell
    // sizes from 2^0 to 2^64.
    std::array<Level, 65> levels;
};

} // namespace svanes

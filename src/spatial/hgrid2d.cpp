#include <svanes/spatial/hgrid2d.hpp>
#include <svanes/utility/hash.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <unordered_set>

namespace svanes {

/**
 * Validates that the provided rectangle bounds are finite and have nonnegative
 * dimensions.
 *
 * @param bounds The Rectangle2D object to validate.
 *
 * @throws std::invalid_argument if any of the bounds are not finite or if the
 * width or height is negative.
 */
static void ValidateBounds(const Rectangle2D &bounds) {
    if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
        !std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
        bounds.width < 0.0F || bounds.height < 0.0F) {
        throw std::invalid_argument("HGrid bounds require finite coordinates "
                                    "and nonnegative dimensions.");
    }
}

/**
 * Given a world coordinate (either x or y), and the width of a cell,
 * computes the corresponding cell coordinate in the hierarchical grid.
 *
 * @param position The position to compute the cell coordinate for.
 * @param cell_width The width of each cell.
 *
 * @return The cell coordinate, or std::nullopt if the coordinate is out of
 * range.
 */
static std::optional<std::int64_t> CellCoordinate(double position,
                                                  double cell_width) {

    const double coordinate = std::floor(position / cell_width);

    // Anything outside the range [-2^63, 2^63) cannot be represented as an
    // int64_t.
    const double limit = std::ldexp(1.0, 63);
    if (coordinate < -limit || coordinate >= limit) {
        return std::nullopt;
    }

    return static_cast<std::int64_t>(coordinate);
}

/**
 * Determines whether two axis-aligned bounding boxes (AABBs) intersect.
 *
 * @param a The first rectangle to test for intersection.
 * @param b The second rectangle to test for intersection.
 *
 * @return true if the rectangles intersect, false otherwise.
 */
static bool Intersects(const Rectangle2D &a, const Rectangle2D &b) {

    // a.x - a.width/2 gives the left edge of rectangle a, and a.x + a.width/2
    // gives the right edge. Likewise for b, and similarly for the y and height
    // dimensions. The rectangles intersect if any edge of one rectangle is
    // within the bounds of the other rectangle in both the x and y dimensions.
    return static_cast<double>(a.x) - a.width * 0.5 <=
               static_cast<double>(b.x) + b.width * 0.5 &&
           static_cast<double>(b.x) - b.width * 0.5 <=
               static_cast<double>(a.x) + a.width * 0.5 &&
           static_cast<double>(a.y) - a.height * 0.5 <=
               static_cast<double>(b.y) + b.height * 0.5 &&
           static_cast<double>(b.y) - b.height * 0.5 <=
               static_cast<double>(a.y) + a.height * 0.5;
}

std::size_t HGrid2D::CellHash::operator()(const Cell &cell) const {
    // Map the 64-bit x and y coordinates of the cell to a 128-bit space,
    // then turn that into 4 32-bit words to be hashed by Hash.
    const auto x = static_cast<std::uint64_t>(cell.x);
    const auto y = static_cast<std::uint64_t>(cell.y);

    const std::array<std::uint32_t, 4> words{
        static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(x >> 32),
        static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(y >> 32)};

    return Hash(words);
}

void HGrid2D::Rebuild(std::span<const HGridEntry2D> input) {
    // HGrid2D instance to hold the rebuilt structure.
    // Allows us to avoid modifying the current instance until the rebuild is
    // complete.
    HGrid2D rebuilt;
    rebuilt.entries.assign_range(input);

    // Used to prevent duplicate entity IDs in the input. Each entity ID must be
    // unique.
    std::unordered_set<Entity> entities;
    entities.reserve(input.size());

    for (std::size_t i = 0; i < rebuilt.entries.size(); ++i) {
        const HGridEntry2D &entry = rebuilt.entries[i];
        ValidateBounds(entry.bounds);
        if (!entities.insert(entry.entity).second) {
            throw std::invalid_argument(
                "HGrid entries require unique entity IDs.");
        }

        const double dimension =
            std::max(entry.bounds.width, entry.bounds.height);
        if (dimension > std::ldexp(1.0, 64)) {
            throw std::invalid_argument(
                "HGrid entry dimensions cannot exceed 2^64.");
        }

        // We choose the smallest cell size that is at least as large as both
        // dimensions of an object's AABB. For example, a rectangle five units
        // wide and two units tall belongs to level 3 (2^3 = 8 units). A
        // rectangle exactly eight units wide also fits there. One slightly
        // wider than eight belongs to level 4 (2^4 = 16 units).
        //
        // An object appears at only one level, in only one cell. Also, those
        // rectangles whose width and height are less than 2^0 = 1 unit are
        // placed in level 0.

        std::size_t level = 0;
        double cell_width = 1.0;
        while (cell_width < dimension) {
            ++level;
            cell_width = std::ldexp(1.0, static_cast<std::int32_t>(level));
        }

        const auto x = CellCoordinate(entry.bounds.x, cell_width);
        const auto y = CellCoordinate(entry.bounds.y, cell_width);
        if (!x || !y) {
            throw std::overflow_error(
                "HGrid entry cell coordinates exceed int64_t.");
        }
        rebuilt.levels[level][Cell{*x, *y}].push_back(i);
    }

    entries.swap(rebuilt.entries);
    levels.swap(rebuilt.levels);
}

// Could have done the exact same thing with a normal function that instead
// took in a function pointer to a visitor. However, there are two reasons
// why we use a template here instead:
// 1. The visitor can be a lambda, meaning I as the programmer don't need to
// figure out how to pass in the lambda's capture list to an actual function.
// 2. From what I've read, the compiler can optimize this better via inlining,
// although I think that's a load of malarkey. Really all I care about is the
// fact that I can use a lambda.
//
// Also, this pattern is "the C++ way" to do things, it is used by the C++
// standard. For instace: https://eel.is/c++draft/alg.foreach a for each
// algorithm is something that iterates over a range and applies a function to
// each element. As it is implemented by the official C++ standard, a template
// is used, not a function pointer.

template <typename Visitor>
void HGrid2D::VisitCells(std::size_t level_index, const Rectangle2D &bounds,
                         const Visitor &visit) const {

    const Level &level = levels[level_index];

    // We are only interested in levels that contain entries.
    if (level.empty()) {
        return;
    }

    // This level is not empty. First, we want to figure out how big the
    // cells are at this level.
    const double cell_width =
        std::ldexp(1.0, static_cast<std::int32_t>(level_index));

    // Now, we want to figure out which cells in this level could contain
    // entries that intersect the query rectangle. We can do this by
    // creating a new implicit rectangle with the same center, whose width
    // is the width of the query rectangle plus the width of a cell, and
    // whose height is likewise the height of the query rectangle plus the
    // height of a cell.
    //
    // Why don't we just use the query rectangle itself? An entry is stored
    // in the cell that contains its center, but its rectangle can extend
    // outside that cell (imagine that part of the rectangle, up to half of
    // its width and/or height, extend beyond the cell into some adjacent
    // cell). If we used the query rectangle itself, we could miss entries
    // whose centers are in cells outside the query rectangle, even though
    // their rectangles intersect it. Each entry in this level has a width
    // and height no greater than those of a cell, so its rectangle extends
    // at most half a cell width from its center in each direction. By
    // extending the query rectangle by half a cell width in all directions,
    // we ensure that we find all cells containing entries that could
    // intersect the query rectangle.
    const double half_width = bounds.width * 0.5 + cell_width * 0.5;
    const double half_height = bounds.height * 0.5 + cell_width * 0.5;
    const auto min_x = CellCoordinate(bounds.x - half_width, cell_width);
    const auto max_x = CellCoordinate(bounds.x + half_width, cell_width);
    const auto min_y = CellCoordinate(bounds.y - half_height, cell_width);
    const auto max_y = CellCoordinate(bounds.y + half_height, cell_width);


    // If any of the min/max coordinates are out of range,
    // we fall back to iterating over all cells in the level.
    // Also, as an optimization, if the number of cells in the range is
    // greater than the number of cells in the level, we again fall back to
    // iterating over all cells in the level. This will skip querying some
    // (and potentially several orders of magnitude more) cells which are
    // guaranteed to not contain any entries.
    if (!min_x || !max_x || !min_y || !max_y ||
        (static_cast<double>(*max_x) - static_cast<double>(*min_x) + 1.0) *
                (static_cast<double>(*max_y) - static_cast<double>(*min_y) +
                 1.0) >
            static_cast<double>(level.size())) {
        for (const auto &[cell, indices] : level) {
            visit(indices);
        }
        return;
    }

    // The core logic of the query: iterate over all cells in the range
    // defined by the min/max coordinates, and for each cell, check if it is
    // not empty. If so, we should check if any of the entries in that cell
    // intersect the query rectangle, and take note of any entries that do
    // intersect.
    for (std::int64_t y = *min_y;; ++y) {
        for (std::int64_t x = *min_x;; ++x) {
            const auto found = level.find(Cell{x, y});
            if (found != level.end()) {
                visit(found->second);
            }
            if (x == *max_x) {
                break;
            }
        }
        if (y == *max_y) {
            break;
        }
    }
}

std::vector<Entity> HGrid2D::Query(const Rectangle2D &bounds) const {
    ValidateBounds(bounds);
    std::vector<Entity> matches;

    // Level vistor function to check if the entries at the given indices
    // intersect the query rectangle, and if so, append their entity IDs to the
    // matches vector.
    const auto append_matches = [&](const std::vector<std::size_t> &indices) {
        for (std::size_t index : indices) {
            const HGridEntry2D &entry = entries[index];
            if (Intersects(bounds, entry.bounds)) {
                matches.push_back(entry.entity);
            }
        }
    };

    for (std::size_t i = 0; i < levels.size(); ++i) {
        VisitCells(i, bounds, append_matches);
    }
    return matches;
}

std::vector<std::pair<Entity, Entity>> HGrid2D::BuildCollisionPairs() const {

    // General idea: If we query every single entity in the HGrid, we will
    // be wasting several queries on entities that have already been checked,
    // just by a different entity. For instance, if A is colliding with B,
    // and we query A, we already found that we're colliding with B.
    // We do still need to check B, but only if it's colliding with some other
    // entity C that A is not colliding with.
    //
    // So let's query from the ground up. When querying an entity on level i,
    // we will only check for collisions with entities on levels i and above.
    // You can see why this works via an inductive argument.
    // In the base case, we are on the lowest level, level 0.
    // Everything is above us, so we check everything. Now, assume that we are
    // on level i+1, and we have already checked all entities on levels 0
    // through i.  Any collision between an entity on level i+1 and an entity on
    // one of those lower levels would have already been found when we checked
    // the lower entity, since level i+1 was above it. So we don't need to check
    // those lower levels again. We only need to check level i+1 and above,
    // which is the same rule we started with.
    //
    // This handles pairs of entities on different levels. For entities on
    // the same level, we still need to avoid checking both A against B and
    // B against A. We do this by only checking entities whose index in the
    // entries vector is greater than the current entity's index.
    // This also prevents an entity from checking itself.
    //
    // So every pair is considered once: from the lower level if the entities
    // are on different levels, or from the lower index if they are on the
    // same level.
    //


    // List of levels that contain at least one entry.
    // Allows us to skip querying any of the 65 levels that are empty.
    std::vector<std::size_t> occupied_levels;
    for (std::size_t i = 0; i < levels.size(); ++i) {
        if (!levels[i].empty()) {
            occupied_levels.push_back(i);
        }
    }

    // List of pairs of entity IDs to pass to the narrow phase
    // collision detection. Will contain every pair of
    // entities that the broad phase has determined could potentially collide.
    std::vector<std::pair<Entity, Entity>> pairs;

    // For every occupied level starting from the lowest...
    for (std::size_t i = 0; i < occupied_levels.size(); ++i) {
        // ... and for every cell in that level...
        for (const auto &[cell, indices] : levels[occupied_levels[i]]) {
            // ... and for every entity in that cell...
            for (std::size_t a : indices) {

                // Find all entities on the same level or above that could
                // potentially collide with the current entity.

                for (std::size_t j = i; j < occupied_levels.size(); ++j) {
                    VisitCells(occupied_levels[j], entries[a].bounds,
                               [&](const std::vector<std::size_t> &candidates) {
                                   for (std::size_t b : candidates) {
                                       // For every entity in the same level
                                       // that could potentially collide with
                                       // the current entity, it's a candidate
                                       // for narrow phase collision detection
                                       // if its index in the input vector is
                                       // greater than the current entity's
                                       // index. This ensures that we only check
                                       // each pair of entities in the same
                                       // level once in the narrow phase, and
                                       // that we don't check an entity against
                                       // itself.
                                       if (i == j && b <= a) {
                                           continue;
                                       }

                                       // Broad phase collision detection: is
                                       // satisfied if the AABBs of the two
                                       // entities intersect.
                                       if (Intersects(entries[a].bounds,
                                                      entries[b].bounds)) {
                                           const Entity entity_a =
                                               entries[a].entity;
                                           const Entity entity_b =
                                               entries[b].entity;

                                           // For consistent narrow phase
                                           // results, we always put the smaller
                                           // entity ID first in the pair.
                                           pairs.emplace_back(
                                               std::min(entity_a, entity_b),
                                               std::max(entity_a, entity_b));
                                       }
                                   }
                               });
                }
            }
        }
    }

    return pairs;
}

} // namespace svanes

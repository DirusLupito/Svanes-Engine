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

std::vector<Entity> HGrid2D::Query(const Rectangle2D &bounds) const {
    ValidateBounds(bounds);
    std::vector<Entity> matches;

    // Helper to check if the entries at the given indices intersect the query
    // rectangle, and if so, append their entity IDs to the matches vector.
    const auto append_matches = [&](const std::vector<std::size_t> &indices) {
        for (std::size_t index : indices) {
            const HGridEntry2D &entry = entries[index];
            if (Intersects(bounds, entry.bounds)) {
                matches.push_back(entry.entity);
            }
        }
    };

    for (std::size_t i = 0; i < levels.size(); ++i) {
        const Level &level = levels[i];

        // We are only interested in levels that contain entries.
        if (level.empty()) {
            continue;
        }

        // This level is not empty. First, we want to figure out how big the
        // cells are at this level.
        const double cell_width = std::ldexp(1.0, static_cast<std::int32_t>(i));

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
                append_matches(indices);
            }
            continue;
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
                    append_matches(found->second);
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
    return matches;
}

} // namespace svanes

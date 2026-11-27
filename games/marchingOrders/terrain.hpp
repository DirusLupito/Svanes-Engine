#pragma once

#include <cstdint>
#include <vector>

namespace marching_orders {

/**
 * Provides read-only access to the map's elevation in whole meters.
 * Cell (0, 0) is at the top left. Columns increase to the right and rows
 * increase downward.
 */
class Terrain {
public:
    /**
     * Loads the terrain on first use and shares it for the life of the process.
     *
     * @return The elevation grid for the map.
     *
     * @throws std::runtime_error if the elevation data is unavailable or
     * invalid.
     * @throws std::length_error if the elevation data is too large to load.
     */
    static const Terrain &Get();

    /**
     * Reports the horizontal extent of the grid.
     *
     * @return The number of columns.
     */
    std::int32_t Width() const;

    /**
     * Reports the vertical extent of the grid.
     *
     * @return The number of rows.
     */
    std::int32_t Height() const;

    /**
     * Reads the elevation of a terrain cell.
     *
     * @param x Zero-based column in the grid.
     * @param y Zero-based row in the grid.
     *
     * @return Signed elevation in meters relative to sea level.
     *
     * @throws std::out_of_range if either coordinate is outside the grid.
     */
    std::int16_t ElevationAt(std::int32_t x, std::int32_t y) const;

private:
    /**
     * Decodes the packaged elevation samples into the grid.
     */
    Terrain();

    /**
     * Deleted copy constructor to keep readers from duplicating the shared
     * elevation grid.
     *
     * This prevents something like:
     * Terrain copy = Terrain::Get();
     */
    Terrain(const Terrain &) = delete;

    /**
     * Deleted copy assignment operator to prevent replacing a terrain's
     * elevation grid with a copy.
     *
     * This prevents something like:
     * void CopyTerrain(Terrain &target, const Terrain &source) {
     *     target = source;
     * }
     */
    Terrain &operator=(const Terrain &) = delete;

    // Elevations in meters, stored row by row.
    std::vector<std::int16_t> elevations;
};

} // namespace marching_orders

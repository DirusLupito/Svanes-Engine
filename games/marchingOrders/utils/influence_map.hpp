#pragma once

#include <svanes/vector2d.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace marching_orders {

/**
 * Implementation of an influence map.
 * Used to track variable values across a game space.
 */
class InfluenceMap {
public:
    /**
     * Constructs an influence map with the provided dimensions.
     * 
     * @param width The width of the influence map in cells.
     * @param height The height of the influence map in cells.
     * @param cell_size The size of each cell in world units.
     * @param origin Anchor point of the map in the world.
     *
     * @throws std::invalid_argument if any of the provided dimensions are negative/infinite.
     * @throws std::length_error if the provided dimensions exceed maximum vector size.
     */
    InfluenceMap(std::uint32_t width, std::uint32_t height, float cell_size,
                 svanes::Vector2D origin);

    /**
     * Resets all cells in the map to zero.
     */
    void Clear();

    /**
     * Applies a half-life to values across the map. Clamps values to zero 
     * once they are below .0001. This function is to be called as a part of
     * an update cycle.
     * 
     * @param elapsed_seconds The time in seconds since the last time Fade() has been executed.
     * @param half_life_seconds The half life period in seconds.
     *
     * @throws std::invalid_argument if elapsed time or half life are negative or infinite.
     */
    void Fade(float elapsed_seconds, float half_life_seconds);

    /**
     * Function for updating values on the map. A stamp is applied at a certain
     * location, radiating out in a circle and decaying based on a provided callback
     * function. The decay callback expects to receive the source of the stamp, the
     * current cell, and the normalized distance between the two. It should return a
     * multiplier that will be applied to the value to calculate decay.
     * 
     * @param location The location in the map that will be the center of the stamp.
     * @param value The value to be set at the center of the stamp.
     * @param max_radius The radius defining the maximum range that the stamp should reach.
     * @param decay The callback defining how propagated values should decay over distance.
     *
     * @throws std::invalid_argument if the dimensions are not finite, if the max radius is negative or infinite, or if the decay funciton is invalid.
     */
    void Stamp(svanes::Vector2D location, float value, float max_radius,
               const std::function<float(svanes::Vector2D, svanes::Vector2D,
                                         float)> &decay);

    /**
     * Returns the value in a cell given its coordinates.
     * 
     * @param x The x coordinate of the cell being queried.
     * @param y The y coordinate of the cell being queried.
     *
     * @return The value at the queried cell.
     *
     * @throws std::out_of_range if the queried coordinates are out of bounds.
     */
    float ReadCell(std::uint32_t x, std::uint32_t y) const;

    /**
     * Accepts a world coordinate and queries the corresponding map cell.
     * 
     * @param position The world position being queried.
     *
     * @return The value at the corresponding map cell.
     *
     * @throws std::invalid_argument if the map position is infinite
     * @throws std::out_of_range if the converted coordinates fall outside of the map's bounds.
     */
    float ReadWorld(svanes::Vector2D position) const;

private:
    // The width of the map in cells
    std::uint32_t width;

    // The height of the map in cells.
    std::uint32_t height;

    // The size of the cells in world units.
    float cell_size;

    // The anchor point of the map.
    svanes::Vector2D origin;

    // The values at each point of the grid.
    std::vector<float> values;
};

}

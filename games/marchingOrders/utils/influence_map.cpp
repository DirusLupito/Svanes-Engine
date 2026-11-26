#include "influence_map.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace marching_orders {

InfluenceMap::InfluenceMap(std::uint32_t width, std::uint32_t height,
                           float cell_size, svanes::Vector2D origin)
    : width(width), height(height), cell_size(cell_size), origin(origin) {
    if (width == 0 || height == 0 || !std::isfinite(cell_size) ||
        cell_size <= 0.0F || !std::isfinite(origin.x) ||
        !std::isfinite(origin.y)) {
        throw std::invalid_argument(
            "Influence map requires positive dimensions, a finite positive "
            "cell size, and a finite origin.");
    }
    if (width > values.max_size() / height) {
        throw std::length_error(
            "Influence map dimensions exceed grid capacity.");
    }
    values.resize(static_cast<std::size_t>(width) * height, 0.0F);
}

void InfluenceMap::Clear() { std::fill(values.begin(), values.end(), 0.0F); }

void InfluenceMap::Fade(float elapsed_seconds, float half_life_seconds) {
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0F ||
        !std::isfinite(half_life_seconds) || half_life_seconds <= 0.0F) {
        throw std::invalid_argument(
            "Fade requires finite nonnegative elapsed time and a finite "
            "positive half-life.");
    }
    if (elapsed_seconds == 0.0F) {
        return;
    }
    const float multiplier = static_cast<float>(
        std::exp2(-static_cast<double>(elapsed_seconds) / half_life_seconds));
    for (auto &value : values) {
        value *= multiplier;
        if (std::abs(value) <= 0.001F) {
            value = 0.0F;
        }
    }
}

void InfluenceMap::Stamp(
    svanes::Vector2D location, float value, float max_radius,
    const std::function<float(svanes::Vector2D, svanes::Vector2D, float)>
        &decay) {
    if (!decay || !std::isfinite(location.x) || !std::isfinite(location.y) ||
        !std::isfinite(value) || !std::isfinite(max_radius) ||
        max_radius <= 0.0F) {
        throw std::invalid_argument(
            "Stamp requires a decay callback, finite location and value, "
            "and a finite positive maximum radius.");
    }
    // Calculate the area that could potentially be affected by the stamp.
    // This yields a rectangle that contains the entire area of the stamp
    // circle within it.
    const auto bounds = [this, max_radius](float center, float axis_origin,
                                           std::uint32_t count) {
        const double low =
            (static_cast<double>(center) - max_radius - axis_origin) /
            cell_size;
        const double high =
            (static_cast<double>(center) + max_radius - axis_origin) /
            cell_size;
        return std::pair{
            static_cast<std::uint32_t>(
                std::clamp(std::floor(low), 0.0, static_cast<double>(count))),
            static_cast<std::uint32_t>(
                std::clamp(std::ceil(high), 0.0, static_cast<double>(count)))};
    };
    const auto [begin_x, end_x] = bounds(location.x, origin.x, width);
    const auto [begin_y, end_y] = bounds(location.y, origin.y, height);
    // Loop through the bounds and calculate values
    for (std::uint32_t y = begin_y; y < end_y; ++y) {
        for (std::uint32_t x = begin_x; x < end_x; ++x) {
            const auto index = static_cast<std::size_t>(y) * width + x;
            // Check that this cell is within the circle area
            const svanes::Vector2D position{
                origin.x + (static_cast<float>(x) + 0.5F) * cell_size,
                origin.y + (static_cast<float>(y) + 0.5F) * cell_size};
            const double distance =
                std::hypot(static_cast<double>(position.x) - location.x,
                           static_cast<double>(position.y) - location.y);
            if (distance > max_radius) {
                continue;
            }
            // Use the callback to calculate decay
            const float multiplier = decay(
                location, position, static_cast<float>(distance / max_radius));
            // Apply the updated value
            const float updated_value = values[index] + value * multiplier;
            if (!std::isfinite(multiplier) || !std::isfinite(updated_value)) {
                throw std::invalid_argument(
                    "Stamp decay and accumulated influence must be finite.");
            }
            values[index] = updated_value;
        }
    }
}

float InfluenceMap::ReadCell(std::uint32_t x, std::uint32_t y) const {
    if (x >= width || y >= height) {
        throw std::out_of_range("Influence map cell is outside the grid.");
    }
    return values[static_cast<std::size_t>(y) * width + x];
}

float InfluenceMap::ReadWorld(svanes::Vector2D position) const {
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        throw std::invalid_argument(
            "Influence map read position must be finite.");
    }
    // Calculate index from the provided world position
    const double x = (static_cast<double>(position.x) - origin.x) / cell_size;
    const double y = (static_cast<double>(position.y) - origin.y) / cell_size;
    if (x < 0.0 || y < 0.0 || x >= width || y >= height) {
        throw std::out_of_range(
            "Influence map read position is outside the grid.");
    }
    // Return the value at the calculated index
    return ReadCell(static_cast<std::uint32_t>(std::floor(x)),
                    static_cast<std::uint32_t>(std::floor(y)));
}

}

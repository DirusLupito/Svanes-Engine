#include "terrain.hpp"

#include <svanes/asset_path.hpp>
#include <svanes/utility/file_io.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

/**
 * Interpolates between two terrain colors, one RGB channel at a time.
 *
 * @param low The color at the start of the interval.
 * @param high The color at the end of the interval.
 * @param amount Fraction of the interval from low (0) to high (1).
 *
 * @return The interpolated color with full opacity.
 */
static svanes::Color MixColors(svanes::Color low, svanes::Color high,
                               float amount) {
    const auto blend = [amount](float a, float b) {
        return static_cast<std::uint8_t>(std::lerp(a, b, amount));
    };
    return {blend(low.red, high.red), blend(low.green, high.green),
            blend(low.blue, high.blue)};
}

/**
 * Colors water by depth and land by its estimated temperature. Cells at or
 * below sea level use the water palette, including inland depressions.
 *
 * @param height_meters Elevation in meters relative to sea level.
 * @param shade Brightness multiplier for land. A value of 1 keeps the
 * original palette color.
 *
 * @return The map color for this cell.
 */
static svanes::Color TerrainColor(float height_meters, float shade) {
    if (height_meters <= 0.0F) {
        // The square root gives shallow coastal waters more of the color range.
        const float depth_fraction = std::sqrt(-height_meters / 1300.0F);
        return MixColors({105, 174, 178}, {33, 88, 121},
                         std::clamp(depth_fraction, 0.0F, 1.0F));
    }

    constexpr float sea_level_celsius = 30.0F;
    constexpr float cooling_per_meter = 0.0065F;
    constexpr float upland_celsius = 16.0F;
    const float temperature =
        sea_level_celsius - cooling_per_meter * height_meters;
    constexpr svanes::Color lowland{222, 200, 151};
    constexpr svanes::Color upland{146, 159, 122};
    constexpr svanes::Color ridge{203, 201, 185};
    svanes::Color land;
    if (temperature >= upland_celsius) {
        const float warmth = (temperature - upland_celsius) /
                             (sea_level_celsius - upland_celsius);
        land = MixColors(upland, lowland, warmth);
    } else {
        land = MixColors(ridge, upland,
                         std::clamp(temperature / upland_celsius, 0.0F, 1.0F));
    }
    const auto lit = [shade](std::uint8_t channel) {
        return static_cast<std::uint8_t>(
            std::clamp(channel * shade, 0.0F, 255.0F));
    };
    return {lit(land.red), lit(land.green), lit(land.blue)};
}

svanes::ImageData marching_orders::LoadTerrainImage() {
    constexpr std::int32_t terrain_side = 880;
    const auto bytes = svanes::ReadFile(
        svanes::AssetPath("assets/marchingOrders/southern_red_sea.bil"));
    constexpr std::size_t cells = terrain_side * terrain_side;
    if (bytes.size() != 2 * cells) {
        throw std::runtime_error(
            "Unexpected size for the southern Red Sea elevation grid.");
    }

    // The Oceania/generic mapping tools data apparently stores signed 
    // 16-bit elevations, low byte first so we decode them explicitly here
    // just in case other clients/the server would disagree on endianness.

    std::vector<std::int16_t> heights(cells);
    for (std::size_t i = 0; i < cells; ++i) {
        const auto low_byte = std::to_integer<std::uint8_t>(bytes[2 * i]);
        const auto high_byte = std::to_integer<std::uint8_t>(bytes[2 * i + 1]);
        const auto value =
            static_cast<std::uint16_t>(low_byte | (high_byte << 8));
        heights[i] = std::bit_cast<std::int16_t>(value);
    }

    svanes::ImageData image{.width = terrain_side, .height = terrain_side};
    image.rgba_pixels.resize(cells * 4);
    for (std::int32_t y = 0; y < terrain_side; ++y) {
        for (std::int32_t x = 0; x < terrain_side; ++x) {
            const auto i = y * terrain_side + x;

            // 
            // POST PROCESSING: LIGHTING BAKED INTO THE TERRAIN IMAGE
            // 

            // Shade the terrain texture so ridges are visible on the flat map.
            // Land above the average height of its north and west neighbors
            // appears lighter. Land below it appears darker. At an edge, the
            // cell serves as its own neighbor so that height difference is
            // zero.

            const auto west = heights[y * terrain_side + std::max(0, x - 1)];
            const auto north = heights[std::max(0, y - 1) * terrain_side + x];
            const auto rise = 2 * heights[i] - west - north;
            const float shade =
                1.0F + std::clamp(rise / 1050.0F, -0.26F, 0.22F);
            const auto color = TerrainColor(heights[i], shade);
            const auto pixel = 4 * i;
            image.rgba_pixels[pixel] = color.red;
            image.rgba_pixels[pixel + 1] = color.green;
            image.rgba_pixels[pixel + 2] = color.blue;
            image.rgba_pixels[pixel + 3] = color.alpha;
        }
    }
    return image;
}

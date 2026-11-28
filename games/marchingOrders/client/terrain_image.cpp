#include "terrain_image.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

// Number of consecutive channel bytes occupied by one RGBA pixel.
constexpr std::size_t kChannelsPerPixel = 4;

// Reference elevation in meters for water shading and land temperature.
constexpr float kSeaLevelMeters = 0.0F;

// Depth below sea level, in meters, where the deep-water color is reached.
constexpr float kDeepWaterDepthMeters = 1300.0F;

// Water color at or above sea level.
constexpr svanes::Color kShallowWaterColor{
    .red = 105, .green = 174, .blue = 178};

// Water color used at and beyond the deep-water depth.
constexpr svanes::Color kDeepWaterColor{.red = 33, .green = 88, .blue = 121};

// Reference temperature in degrees Celsius at sea level.
constexpr float kSeaLevelCelsius = 30.0F;

// Temperature drop in degrees Celsius for each meter of ascent.
constexpr float kCoolingCelsiusPerMeter = 0.0065F;

// Temperature in degrees Celsius where the land palette reaches the upland
// color.
constexpr float kUplandCelsius = 16.0F;

// Temperature in degrees Celsius at or below which land takes the ridge color.
constexpr float kRidgeCelsius = 0.0F;

// Base color for land at the warm end of the temperature range.
constexpr svanes::Color kLowlandColor{.red = 146, .green = 159, .blue = 122};

// Base color for land at the transition between lowlands and ridges.
constexpr svanes::Color kUplandColor{.red = 136, .green = 138, .blue = 135};

// Base color for land at the cold end of the temperature range.
constexpr svanes::Color kRidgeColor{.red = 203, .green = 201, .blue = 185};

// Reference rise in meters. Increasing it reduces the contrast of a given
// slope.
constexpr float kHillshadeRiseScaleMeters = 1050.0F;

// Most negative brightness adjustment, expressed as a fraction of the base
// color.
constexpr float kMinHillshadeAdjustment = -0.26F;

// Largest brightness increase, expressed as a fraction of the base color.
constexpr float kMaxHillshadeAdjustment = 0.22F;

} // namespace

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
 * Colors a terrain cell according to its mapped surface.
 *
 * @param is_water True if the water mask covers this cell.
 * @param height_meters Elevation in meters relative to sea level.
 * @param shade Brightness multiplier for land. A value of 1 keeps the
 * original palette color.
 *
 * @return The map color for this cell.
 */
static svanes::Color TerrainColor(bool is_water, float height_meters,
                                  float shade) {
    const float height_above_sea = height_meters - kSeaLevelMeters;
    if (is_water) {
        // The square root gives shallow coastal waters more of the color range.
        const float depth_fraction =
            std::clamp(-height_above_sea / kDeepWaterDepthMeters, 0.0F, 1.0F);
        return MixColors(kShallowWaterColor, kDeepWaterColor,
                         std::sqrt(depth_fraction));
    }

    const float temperature =
        kSeaLevelCelsius - kCoolingCelsiusPerMeter * height_above_sea;
    svanes::Color land;
    if (temperature >= kUplandCelsius) {
        const float warmth = (temperature - kUplandCelsius) /
                             (kSeaLevelCelsius - kUplandCelsius);
        land = MixColors(kUplandColor, kLowlandColor, std::min(warmth, 1.0F));
    } else {
        const float warmth =
            (temperature - kRidgeCelsius) / (kUplandCelsius - kRidgeCelsius);
        land = MixColors(kRidgeColor, kUplandColor,
                         std::clamp(warmth, 0.0F, 1.0F));
    }
    const auto lit = [shade](std::uint8_t channel) {
        return static_cast<std::uint8_t>(std::clamp<float>(
            channel * shade, 0.0F, std::numeric_limits<std::uint8_t>::max()));
    };
    return {lit(land.red), lit(land.green), lit(land.blue)};
}

svanes::ImageData marching_orders::LoadTerrainImage(const Terrain &terrain) {
    svanes::ImageData image{.width = terrain.Width(),
                            .height = terrain.Height()};
    const auto cells = static_cast<std::size_t>(image.width) * image.height;
    image.rgba_pixels.resize(cells * kChannelsPerPixel);
    for (std::int32_t y = 0; y < image.height; ++y) {
        for (std::int32_t x = 0; x < image.width; ++x) {
            const auto i = static_cast<std::size_t>(y) * image.width + x;

            //
            // POST PROCESSING: LIGHTING BAKED INTO THE TERRAIN IMAGE
            //

            // Shade the terrain texture so ridges are visible on the flat map.
            // Land above the average height of its north and west neighbors
            // appears lighter. Land below it appears darker. At an edge, the
            // cell serves as its own neighbor so that height difference is
            // zero.

            const auto height = terrain.ElevationAt(x, y);
            const auto west = terrain.ElevationAt(std::max(0, x - 1), y);
            const auto north = terrain.ElevationAt(x, std::max(0, y - 1));
            // Sum of the elevation gains from the two neighbors, in meters.
            const auto rise = 2 * height - west - north;
            const float shade =
                1.0F + std::clamp(rise / kHillshadeRiseScaleMeters,
                                  kMinHillshadeAdjustment,
                                  kMaxHillshadeAdjustment);
            const auto color =
                TerrainColor(terrain.IsWaterAt(x, y), height, shade);
            const auto pixel = kChannelsPerPixel * i;
            image.rgba_pixels[pixel] = color.red;
            image.rgba_pixels[pixel + 1] = color.green;
            image.rgba_pixels[pixel + 2] = color.blue;
            image.rgba_pixels[pixel + 3] = color.alpha;
        }
    }
    return image;
}

#include "terrain.hpp"

#include <svanes/asset_path.hpp>
#include <svanes/utility/file_io.hpp>

#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace marching_orders {

// Path to the grid of terrain elevations, relative to the executable.
constexpr std::string_view kElevationGridPath =
    "assets/marchingOrders/southern_red_sea.bil";

// Samples along each edge of the packaged square elevation grid.
constexpr std::int32_t kTerrainSide = 880;

Terrain::Terrain() {
    const auto bytes = svanes::ReadFile(svanes::AssetPath(kElevationGridPath));
    const auto cells = static_cast<std::size_t>(Width()) * Height();
    if (bytes.size() != sizeof(std::int16_t) * cells) {
        throw std::runtime_error(
            "Unexpected size for the terrain elevation grid.");
    }

    // The Oceania/generic mapping tools data apparently stores signed
    // 16-bit elevations, low byte first so we decode them explicitly here
    // just in case other clients/the server would disagree on endianness.

    elevations.resize(cells);
    for (std::size_t i = 0; i < cells; ++i) {
        const auto offset = sizeof(std::int16_t) * i;
        const auto low_byte = std::to_integer<std::uint8_t>(bytes[offset]);
        const auto high_byte = std::to_integer<std::uint8_t>(bytes[offset + 1]);
        const auto value = static_cast<std::uint16_t>(
            low_byte |
            (high_byte << std::numeric_limits<std::uint8_t>::digits));
        elevations[i] = std::bit_cast<std::int16_t>(value);
    }
}

const Terrain &Terrain::Get() {
    static const Terrain terrain;
    return terrain;
}

std::int32_t Terrain::Width() const { return kTerrainSide; }

std::int32_t Terrain::Height() const { return kTerrainSide; }

std::int16_t Terrain::ElevationAt(std::int32_t x, std::int32_t y) const {
    if (x < 0 || x >= Width() || y < 0 || y >= Height()) {
        throw std::out_of_range(
            "Terrain cell coordinates are outside the grid.");
    }
    return elevations[static_cast<std::size_t>(y) * Width() + x];
}

} // namespace marching_orders

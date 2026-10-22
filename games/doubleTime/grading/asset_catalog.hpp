#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

struct DoubleTimeAssetCatalogEntry {
    std::string_view name;
    std::string_view music_filename;
    std::uint32_t beats_per_minute;
    std::vector<std::string_view> tilemap_filenames;
    std::optional<std::size_t> next_area;
};

// Area metadata lives here so the game and server do not need to encode level
// filenames or progression rules in their update loops.
inline const std::vector<DoubleTimeAssetCatalogEntry> &DoubleTimeAssetCatalog() {
    static const std::vector<DoubleTimeAssetCatalogEntry> areas{
        {"OldCity", "audio/music/OldCity.wav", 160,
         {"tilemaps/OldCity/test.png",
          "tilemaps/OldCity/OldCity_2.png"}, std::nullopt},
    };
    return areas;
}

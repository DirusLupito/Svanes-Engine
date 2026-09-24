#pragma once

#include "network_protocol.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <svanes/tilemaps/tilemap.hpp>
#include <svanes/collision_system.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct TileMapColorMapping {
    svanes::Color color;
    svanes::TileMapCell cell;
};

inline svanes::TileMap CreateTileMapFromPng(
    std::string_view image_path, svanes::TextureHandle atlas,
    std::span<const TileMapColorMapping> color_mappings) {
    if (image_path.empty()) {
        throw std::invalid_argument("Tilemap PNG path cannot be empty.");
    }

    const std::string path{image_path};
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> image{
        IMG_Load(path.c_str()), SDL_DestroySurface};
    if (!image) {
        throw std::runtime_error("Could not load tilemap PNG '" + path +
                                 "': " + SDL_GetError());
    }

    svanes::TileMap tile_map{
        .atlas = atlas,
        .columns = static_cast<std::uint32_t>(image->w),
        .rows = static_cast<std::uint32_t>(image->h),
        .tile_width = static_cast<std::uint32_t>(kChrisTileSize),
        .tile_height = static_cast<std::uint32_t>(kChrisTileSize),
        .atlas_tile_width = 80,
        .atlas_tile_height = 80,
        .atlas_columns = 3,
        .atlas_rows = 1,
    };
    tile_map.cells.reserve(static_cast<std::size_t>(tile_map.columns) *
                           tile_map.rows);

    for (std::int32_t y = 0; y < image->h; ++y) {
        for (std::int32_t x = 0; x < image->w; ++x) {
            std::uint8_t red = 0;
            std::uint8_t green = 0;
            std::uint8_t blue = 0;
            std::uint8_t alpha = 0;
            if (!SDL_ReadSurfacePixel(image.get(), x, y, &red, &green, &blue,
                                      &alpha)) {
                throw std::runtime_error(
                    "Could not read tilemap pixel at (" +
                    std::to_string(x) + ", " + std::to_string(y) +"): " +
                    SDL_GetError());
            }

            const svanes::Color pixel{red, green, blue, alpha};
            const auto mapping = std::find_if(
                color_mappings.begin(), color_mappings.end(),
                [&pixel](const TileMapColorMapping &candidate) {
                    return candidate.color.red == pixel.red &&
                           candidate.color.green == pixel.green &&
                           candidate.color.blue == pixel.blue &&
                           candidate.color.alpha == pixel.alpha;
                });
            if (mapping == color_mappings.end()) {
                throw std::invalid_argument(
                    "Unmapped tilemap color at (" + std::to_string(x) + ", " +
                    std::to_string(y) + ").");
            }
            tile_map.cells.push_back(mapping->cell);
        }
    }

    svanes::BuildTileMapCollisionCache(tile_map);
    return tile_map;
}

inline svanes::TileMap CreateDoubleTimeTileMapFromPng(
    std::string_view image_path, svanes::TextureHandle atlas = {}) {
    constexpr std::array color_mappings{
        TileMapColorMapping{{0, 0, 0, 0}, {0, false}},
        TileMapColorMapping{{0, 0, 0, 255}, {1, true}},
        TileMapColorMapping{{255, 0, 0, 255}, {1, true}},
        TileMapColorMapping{{0, 0, 255, 255}, {2, false}},
        TileMapColorMapping{{255, 255, 0, 255}, {2, true}},
        TileMapColorMapping{{0, 255, 0, 255}, {3, false}},
    };
    return CreateTileMapFromPng(image_path, atlas, color_mappings);
}

inline svanes::Transform GetCenteredTileMapTransform(
    const svanes::TileMap &tile_map, float center_x, float center_y) {
    const float width = static_cast<float>(tile_map.columns) *
                        static_cast<float>(tile_map.tile_width);
    const float height = static_cast<float>(tile_map.rows) *
                         static_cast<float>(tile_map.tile_height);
    return svanes::Transform{center_x - width * 0.5F,
                             center_y - height * 0.5F, 0.0F};
}

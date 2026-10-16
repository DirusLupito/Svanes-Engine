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
#include <optional>
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

struct DoubleTimeMovingPlatform {
    std::uint32_t id = 0;
    std::int32_t start_x = 0;
    std::int32_t start_y = 0;
    std::int32_t end_x = 0;
    std::int32_t end_y = 0;
    std::int32_t width = 1;
    std::int32_t height = 1;
};

struct DoubleTimeDoor {
    std::uint32_t id = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 1;
    std::int32_t height = 1;
};

struct DoubleTimeSpawnPoint {
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct DoubleTimeGoalArea {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 1;
    std::int32_t height = 1;
};

struct DoubleTimeLevelArea {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 1;
    std::int32_t height = 1;
};

struct DoubleTimeLevelData {
    svanes::TileMap tile_map;
    std::vector<DoubleTimeMovingPlatform> moving_platforms;
    std::vector<DoubleTimeDoor> doors;
    std::optional<DoubleTimeSpawnPoint> spawn_point;
    std::optional<DoubleTimeGoalArea> goal;
    std::vector<DoubleTimeLevelArea> death_zones;
    std::vector<DoubleTimeLevelArea> spike_zones;
    std::vector<DoubleTimeLevelArea> collectibles;
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
        .tile_width = static_cast<std::uint32_t>(kDoubleTimeTileSize),
        .tile_height = static_cast<std::uint32_t>(kDoubleTimeTileSize),
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
        TileMapColorMapping{{255, 0, 0, 255}, {0, false}},
        TileMapColorMapping{{0, 0, 255, 255}, {0, false}},
        TileMapColorMapping{{0, 177, 255, 255}, {0, false}},
        TileMapColorMapping{{255, 255, 255, 255}, {0, false}},
        TileMapColorMapping{{200, 0, 255, 255}, {0, false}},
        TileMapColorMapping{{0, 255, 0, 255}, {0, false}},
        TileMapColorMapping{{44, 255, 0, 255}, {0, false}},
        TileMapColorMapping{{0, 74, 0, 255}, {0, false}},
        TileMapColorMapping{{255, 222, 0, 255}, {0, false}},
        TileMapColorMapping{{255, 111, 0, 255}, {0, false}},
    };
    return CreateTileMapFromPng(image_path, atlas, color_mappings);
}

inline DoubleTimeLevelData LoadDoubleTimeLevelData(
    std::string_view image_path, svanes::TextureHandle atlas = {}) {
    const std::string path{image_path};
    std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> image{
        IMG_Load(path.c_str()), SDL_DestroySurface};
    if (!image) {
        throw std::runtime_error("Could not load tilemap PNG '" + path +
                                 "': " + SDL_GetError());
    }

    constexpr svanes::Color kStart{0, 0, 255, 255};
    constexpr svanes::Color kEnd{0, 177, 255, 255};
    constexpr svanes::Color kPath{255, 255, 255, 255};
    constexpr svanes::Color kDoor{0, 74, 0, 255};
    constexpr svanes::Color kSpawn{200, 0, 255, 255};
    constexpr svanes::Color kGoal{44, 255, 0, 255};
    constexpr svanes::Color kDeath{255, 0, 0, 255};
    constexpr svanes::Color kSpikes{255, 111, 0, 255};
    constexpr svanes::Color kCollectible{255, 222, 0, 255};
    const auto is_color = [](const svanes::Color &a, const svanes::Color &b) {
        return a.red == b.red && a.green == b.green && a.blue == b.blue &&
               a.alpha == b.alpha;
    };

    DoubleTimeLevelData result;
    result.tile_map = CreateDoubleTimeTileMapFromPng(image_path, atlas);
    for (std::int32_t y = 0; y < image->h; ++y) {
        for (std::int32_t x = 0; x < image->w; ++x) {
            std::uint8_t r = 0, g = 0, b = 0, a = 0;
            SDL_ReadSurfacePixel(image.get(), x, y, &r, &g, &b, &a);
            const svanes::Color pixel{r, g, b, a};
            if (is_color(pixel, kDeath)) result.death_zones.push_back({x, y, 1, 1});
            if (is_color(pixel, kSpikes)) result.spike_zones.push_back({x, y, 1, 1});
            if (is_color(pixel, kCollectible)) result.collectibles.push_back({x, y, 1, 1});
        }
    }
    std::vector<bool> visited(static_cast<std::size_t>(image->w * image->h));
    std::vector<bool> door_visited(static_cast<std::size_t>(image->w * image->h));
    for (std::int32_t y = 0; y < image->h; ++y) {
        for (std::int32_t x = 0; x < image->w; ++x) {
            std::uint8_t r = 0, g = 0, b = 0, a = 0;
            SDL_ReadSurfacePixel(image.get(), x, y, &r, &g, &b, &a);
            if (is_color(svanes::Color{r, g, b, a}, kSpawn)) {
                result.spawn_point = DoubleTimeSpawnPoint{x, y};
            }
        }
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> goal_cells;
    for (std::int32_t y = 0; y < image->h; ++y) {
        for (std::int32_t x = 0; x < image->w; ++x) {
            std::uint8_t r = 0, g = 0, b = 0, a = 0;
            SDL_ReadSurfacePixel(image.get(), x, y, &r, &g, &b, &a);
            if (is_color(svanes::Color{r, g, b, a}, kGoal)) {
                goal_cells.push_back({x, y});
            }
        }
    }
    if (!goal_cells.empty()) {
        std::int32_t min_x = goal_cells.front().first;
        std::int32_t max_x = min_x;
        std::int32_t min_y = goal_cells.front().second;
        std::int32_t max_y = min_y;
        for (const auto [x, y] : goal_cells) {
            min_x = std::min(min_x, x);
            max_x = std::max(max_x, x);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);
        }
        result.goal = DoubleTimeGoalArea{
            min_x, min_y, max_x - min_x + 1, max_y - min_y + 1};
    }
    std::uint32_t next_id = 0;
    for (std::int32_t y = 0; y < image->h; ++y) {
        for (std::int32_t x = 0; x < image->w; ++x) {
            const std::size_t seed = static_cast<std::size_t>(y * image->w + x);
            if (visited[seed]) continue;
            std::uint8_t r = 0, g = 0, b = 0, a = 0;
            SDL_ReadSurfacePixel(image.get(), x, y, &r, &g, &b, &a);
            const svanes::Color pixel{r, g, b, a};
            if (!is_color(pixel, kStart) && !is_color(pixel, kEnd)) continue;

            std::vector<std::pair<std::int32_t, std::int32_t>> stack{{x, y}};
            visited[seed] = true;
            std::vector<std::pair<std::int32_t, std::int32_t>> starts, ends,
                cells;
            while (!stack.empty()) {
                const auto [cx, cy] = stack.back();
                stack.pop_back();
                std::uint8_t cr = 0, cg = 0, cb = 0, ca = 0;
                SDL_ReadSurfacePixel(image.get(), cx, cy, &cr, &cg, &cb, &ca);
                const svanes::Color current{cr, cg, cb, ca};
                if (is_color(current, kStart)) starts.push_back({cx, cy});
                if (is_color(current, kEnd)) ends.push_back({cx, cy});
                cells.push_back({cx, cy});
                for (const auto [dx, dy] : std::array<std::pair<int, int>, 4>{{
                         {1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                    const std::int32_t nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= image->w || ny >= image->h)
                        continue;
                    const std::size_t index =
                        static_cast<std::size_t>(ny * image->w + nx);
                    if (visited[index]) continue;
                    std::uint8_t nr = 0, ng = 0, nb = 0, na = 0;
                    SDL_ReadSurfacePixel(image.get(), nx, ny, &nr, &ng, &nb,
                                         &na);
                    const svanes::Color neighbor{nr, ng, nb, na};
                    if (is_color(neighbor, kStart) || is_color(neighbor, kEnd) ||
                        is_color(neighbor, kPath)) {
                        visited[index] = true;
                        stack.push_back({nx, ny});
                    }
                }
            }
            if (starts.empty() || ends.empty()) continue;
            const auto bounds = [](const auto &points) {
                std::int32_t min_x = points.front().first;
                std::int32_t max_x = min_x;
                std::int32_t min_y = points.front().second;
                std::int32_t max_y = min_y;
                for (const auto [px, py] : points) {
                    min_x = std::min(min_x, px); max_x = std::max(max_x, px);
                    min_y = std::min(min_y, py); max_y = std::max(max_y, py);
                }
                return std::array<std::int32_t, 4>{min_x, min_y, max_x, max_y};
            };
            const auto start_bounds = bounds(starts);
            const auto end_bounds = bounds(ends);
            result.moving_platforms.push_back(DoubleTimeMovingPlatform{
                next_id++, start_bounds[0], start_bounds[1], end_bounds[0],
                end_bounds[1], start_bounds[2] - start_bounds[0] + 1,
                start_bounds[3] - start_bounds[1] + 1});
        }
    }

    std::uint32_t next_door_id = 0;
    for (std::int32_t y = 0; y < image->h; ++y) {
        for (std::int32_t x = 0; x < image->w; ++x) {
            const std::size_t seed = static_cast<std::size_t>(y * image->w + x);
            if (door_visited[seed]) continue;
            std::uint8_t r = 0, g = 0, b = 0, a = 0;
            SDL_ReadSurfacePixel(image.get(), x, y, &r, &g, &b, &a);
            if (!is_color(svanes::Color{r, g, b, a}, kDoor)) continue;

            std::vector<std::pair<std::int32_t, std::int32_t>> stack{{x, y}};
            door_visited[seed] = true;
            std::int32_t min_x = x, max_x = x, min_y = y, max_y = y;
            while (!stack.empty()) {
                const auto [cx, cy] = stack.back();
                stack.pop_back();
                min_x = std::min(min_x, cx); max_x = std::max(max_x, cx);
                min_y = std::min(min_y, cy); max_y = std::max(max_y, cy);
                for (const auto [dx, dy] : std::array<std::pair<int, int>, 4>{{
                         {1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                    const std::int32_t nx = cx + dx, ny = cy + dy;
                    if (nx < 0 || ny < 0 || nx >= image->w || ny >= image->h)
                        continue;
                    const std::size_t index =
                        static_cast<std::size_t>(ny * image->w + nx);
                    if (door_visited[index]) continue;
                    std::uint8_t nr = 0, ng = 0, nb = 0, na = 0;
                    SDL_ReadSurfacePixel(image.get(), nx, ny, &nr, &ng, &nb,
                                         &na);
                    if (is_color(svanes::Color{nr, ng, nb, na}, kDoor)) {
                        door_visited[index] = true;
                        stack.push_back({nx, ny});
                    }
                }
            }
            result.doors.push_back(DoubleTimeDoor{
                next_door_id++, min_x, min_y, max_x - min_x + 1,
                max_y - min_y + 1});
        }
    }
    return result;
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

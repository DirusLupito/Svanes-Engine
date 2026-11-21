#include "road.hpp"

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace grindset {

namespace {

constexpr float kViewWidth = 1920.0F;
constexpr float kHorizonY = 600.0F;
constexpr std::int32_t kSkyBandCount = 60;

constexpr std::int32_t kSkyZ = 0;
constexpr std::int32_t kSunGlowZ = 1;
constexpr std::int32_t kSunZ = 2;
constexpr std::int32_t kGroundZ = 3;
constexpr std::int32_t kRoadZ = 4;
constexpr std::int32_t kRoadLineZ = 5;

constexpr float kViewHeight = 1080.0F;
constexpr float kVanishingX = kViewWidth * 0.5F;
constexpr float kRoadTopHalfWidth = 4.0F;
constexpr float kRoadBottomHalfWidth = 900.0F;
constexpr float kEdgeLineInner = 0.91F;
constexpr float kEdgeLineOuter = 0.96F;

constexpr svanes::Color kGroundColor{.red = 54, .green = 48, .blue = 58};
constexpr svanes::Color kRoadColor{.red = 36, .green = 36, .blue = 44};
constexpr svanes::Color kRoadLineColor{.red = 214, .green = 204, .blue = 192};

constexpr svanes::Circle2D kSun{
    .x = 720.0F, .y = kHorizonY - 30.0F, .radius = 70.0F};
constexpr float kSunGlowRadius = 360.0F;

/**
 * One color in the sky's vertical fade.
 *
 * FIELDS:
 * - position: How far down the sky the color sits, from 0 at the top of the
 * view to 1 at the horizon.
 * - color: The sky color at that position.
 */
struct SkyStop {
    float position;
    svanes::Color color;
};

constexpr std::array<SkyStop, 4> kSkyStops{{
    {.position = 0.0F, .color = {.red = 22, .green = 30, .blue = 78}},
    {.position = 0.45F, .color = {.red = 120, .green = 72, .blue = 128}},
    {.position = 0.8F, .color = {.red = 238, .green = 132, .blue = 96}},
    {.position = 1.0F, .color = {.red = 255, .green = 204, .blue = 140}},
}};

std::uint8_t Mix(std::uint8_t from, std::uint8_t to, float amount) {
    const float value =
        static_cast<float>(from) +
        (static_cast<float>(to) - static_cast<float>(from)) * amount;
    return static_cast<std::uint8_t>(value + 0.5F);
}

svanes::Color SkyColorAt(float position) {
    for (std::size_t index = 1; index < kSkyStops.size(); ++index) {
        const SkyStop &upper = kSkyStops[index - 1];
        const SkyStop &lower = kSkyStops[index];
        if (position <= lower.position) {
            const float amount =
                (position - upper.position) / (lower.position - upper.position);
            return {.red = Mix(upper.color.red, lower.color.red, amount),
                    .green = Mix(upper.color.green, lower.color.green, amount),
                    .blue = Mix(upper.color.blue, lower.color.blue, amount)};
        }
    }
    return kSkyStops.back().color;
}

void CreateSky(svanes::Registry &world) {
    const float band_height = kHorizonY / static_cast<float>(kSkyBandCount);
    for (std::int32_t band = 0; band < kSkyBandCount; ++band) {
        const float top = band_height * static_cast<float>(band);
        const float position = (static_cast<float>(band) + 0.5F) /
                               static_cast<float>(kSkyBandCount);

        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(
            entity, svanes::Transform{.x = kViewWidth * 0.5F,
                                      .y = top + band_height * 0.5F});
        world.AddComponent<svanes::SolidShape>(
            entity, svanes::SolidShape{.color = SkyColorAt(position),
                                       .geometry = svanes::Rectangle2D{
                                           .width = kViewWidth,
                                           .height = band_height + 1.0F}});
        world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kSkyZ});
    }
}

void CreateSun(svanes::Registry &world) {
    const svanes::Entity glow = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        glow, svanes::Transform{.x = kSun.x, .y = kSun.y});
    world.AddComponent<svanes::RadialGradient2D>(
        glow,
        svanes::RadialGradient2D{
            .geometry = svanes::Circle2D{.radius = kSunGlowRadius},
            .center_color =
                {.red = 255, .green = 214, .blue = 150, .alpha = 150},
            .edge_color = {.red = 255, .green = 160, .blue = 100, .alpha = 0}});
    world.AddComponent<svanes::ZOrder>(glow, svanes::ZOrder{kSunGlowZ});

    const svanes::Entity sun = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        sun, svanes::Transform{.x = kSun.x, .y = kSun.y});
    world.AddComponent<svanes::SolidShape>(
        sun, svanes::SolidShape{
                 .color = {.red = 255, .green = 238, .blue = 196},
                 .geometry = svanes::Circle2D{.radius = kSun.radius}});
    world.AddComponent<svanes::ZOrder>(sun, svanes::ZOrder{kSunZ});
}

void CreatePolygon(svanes::Registry &world,
                   std::vector<svanes::Vector2D> vertices, svanes::Color color,
                   std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{
                    .color = color,
                    .geometry = svanes::ConvexPolygon2D{std::move(vertices)}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
}

void CreateGround(svanes::Registry &world) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = kViewWidth * 0.5F,
                                  .y = (kHorizonY + kViewHeight) * 0.5F});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{.color = kGroundColor,
                                   .geometry = svanes::Rectangle2D{
                                       .width = kViewWidth,
                                       .height = kViewHeight - kHorizonY}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kGroundZ});
}

void CreateEdgeLine(svanes::Registry &world, float side) {
    const float top_inner =
        kVanishingX + side * kRoadTopHalfWidth * kEdgeLineInner;
    const float top_outer =
        kVanishingX + side * kRoadTopHalfWidth * kEdgeLineOuter;
    const float bottom_inner =
        kVanishingX + side * kRoadBottomHalfWidth * kEdgeLineInner;
    const float bottom_outer =
        kVanishingX + side * kRoadBottomHalfWidth * kEdgeLineOuter;
    CreatePolygon(world,
                  {{top_inner, kHorizonY},
                   {top_outer, kHorizonY},
                   {bottom_outer, kViewHeight},
                   {bottom_inner, kViewHeight}},
                  kRoadLineColor, kRoadLineZ);
}

void CreateRoad(svanes::Registry &world) {
    CreatePolygon(world,
                  {{kVanishingX - kRoadTopHalfWidth, kHorizonY},
                   {kVanishingX + kRoadTopHalfWidth, kHorizonY},
                   {kVanishingX + kRoadBottomHalfWidth, kViewHeight},
                   {kVanishingX - kRoadBottomHalfWidth, kViewHeight}},
                  kRoadColor, kRoadZ);
    CreateEdgeLine(world, -1.0F);
    CreateEdgeLine(world, 1.0F);
}

} // namespace

void Road::Initialize(svanes::Registry &world) {
    CreateSky(world);
    CreateSun(world);
    CreateGround(world);
    CreateRoad(world);
}

} // namespace grindset

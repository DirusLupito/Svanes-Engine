#include "road.hpp"

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <array>
#include <cmath>
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

constexpr float kDepthScale = kViewHeight - kHorizonY;
constexpr float kNearestDepth = 0.4F;
constexpr float kHorizonDepth = 150.0F;
constexpr float kDriveSpeed = 15.0F;

constexpr float kRoadHalfWidth = 2.2F;
constexpr float kRoadCenter = -kRoadHalfWidth * 0.5F;
constexpr float kEdgeLineInset = 0.05F;
constexpr float kEdgeLineWidth = 0.1F;
constexpr float kDashLength = 2.5F;
constexpr float kDashPeriod = 10.0F;
constexpr std::size_t kDashCount = 8;
constexpr float kDashHalfWidth = 0.06F;

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

svanes::Entity CreatePolygon(svanes::Registry &world,
                             std::vector<svanes::Vector2D> vertices,
                             svanes::Color color, std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{
                    .color = color,
                    .geometry = svanes::ConvexPolygon2D{std::move(vertices)}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

/**
 * Projects a point on the road onto the screen. Distances are measured in
 * camera heights, so a point one camera height ahead lands on the bottom edge
 * of the view.
 *
 * @param lateral The sideways distance from the camera, positive to the right.
 * @param depth The distance ahead of the camera. Must be positive.
 * @return The point's screen position.
 */
svanes::Vector2D ProjectRoadPoint(float lateral, float depth) {
    return {kVanishingX + lateral * kDepthScale / depth,
            kHorizonY + kDepthScale / depth};
}

/**
 * Builds a strip lying flat on the road, seen in perspective.
 *
 * @param left The strip's left edge, as a sideways distance from the camera.
 * @param right The strip's right edge, as a sideways distance from the camera.
 * @param near_depth The distance ahead of the camera to the strip's near end.
 * @param far_depth The distance ahead of the camera to the strip's far end.
 * @return The strip as a trapezoid narrowing toward the horizon.
 */
svanes::ConvexPolygon2D RoadStrip(float left, float right, float near_depth,
                                  float far_depth) {
    return svanes::ConvexPolygon2D{{ProjectRoadPoint(left, far_depth),
                                    ProjectRoadPoint(right, far_depth),
                                    ProjectRoadPoint(right, near_depth),
                                    ProjectRoadPoint(left, near_depth)}};
}

svanes::ConvexPolygon2D DashShape(float near_depth) {
    return RoadStrip(kRoadCenter - kDashHalfWidth, kRoadCenter + kDashHalfWidth,
                     near_depth, near_depth + kDashLength);
}

float DashNearDepth(std::size_t index, float offset) {
    return kNearestDepth + offset + static_cast<float>(index) * kDashPeriod;
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

void CreateStrip(svanes::Registry &world, float left, float right,
                 svanes::Color color, std::int32_t z_order) {
    const svanes::ConvexPolygon2D shape =
        RoadStrip(left, right, kNearestDepth, kHorizonDepth);
    CreatePolygon(world, shape.Vertices(), color, z_order);
}

void CreateRoad(svanes::Registry &world) {
    const float left = kRoadCenter - kRoadHalfWidth;
    const float right = kRoadCenter + kRoadHalfWidth;
    CreateStrip(world, left, right, kRoadColor, kRoadZ);
    CreateStrip(world, left + kEdgeLineInset,
                left + kEdgeLineInset + kEdgeLineWidth, kRoadLineColor,
                kRoadLineZ);
    CreateStrip(world, right - kEdgeLineInset - kEdgeLineWidth,
                right - kEdgeLineInset, kRoadLineColor, kRoadLineZ);
}

} // namespace

void Road::Initialize(svanes::Registry &world) {
    CreateSky(world);
    CreateSun(world);
    CreateGround(world);
    CreateRoad(world);

    for (std::size_t index = 0; index < kDashCount; ++index) {
        const svanes::ConvexPolygon2D shape =
            DashShape(DashNearDepth(index, dash_offset));
        dashes.push_back(
            CreatePolygon(world, shape.Vertices(), kRoadLineColor, kRoadLineZ));
    }
}

void Road::Update(svanes::Registry &world, float seconds) {
    dash_offset = std::fmod(dash_offset - kDriveSpeed * seconds, kDashPeriod);
    if (dash_offset < 0.0F) {
        dash_offset += kDashPeriod;
    }

    for (std::size_t index = 0; index < dashes.size(); ++index) {
        world.GetComponent<svanes::SolidShape>(dashes[index]).geometry =
            DashShape(DashNearDepth(index, dash_offset));
    }
}

} // namespace grindset

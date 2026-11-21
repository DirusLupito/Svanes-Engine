#include "road.hpp"

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
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
constexpr std::int32_t kPropZ = 6;

constexpr float kViewHeight = 1080.0F;
constexpr float kVanishingX = kViewWidth * 0.5F;

constexpr float kDepthScale = kViewHeight - kHorizonY;
constexpr float kNearestDepth = 0.4F;
constexpr float kHorizonDepth = 150.0F;
constexpr float kDriveSpeed = 15.0F;
constexpr float kCameraHeight = 1.0F;
constexpr float kPatternLength = 240.0F;
constexpr float kPropFadeDepth = 40.0F;

constexpr float kRoadHalfWidth = 2.2F;
constexpr float kRoadCenter = -kRoadHalfWidth * 0.5F;
constexpr float kEdgeLineInset = 0.05F;
constexpr float kEdgeLineWidth = 0.1F;
constexpr float kDashLength = 2.5F;
constexpr float kDashPeriod = 10.0F;
constexpr std::size_t kDashCount = 8;
constexpr float kDashHalfWidth = 0.06F;

constexpr float kTelephoneLateral = kRoadCenter - kRoadHalfWidth - 0.9F;
constexpr float kTelephonePeriod = 12.0F;
constexpr std::size_t kTelephoneCount = 12;
constexpr float kLampLateral = kRoadCenter + kRoadHalfWidth + 0.5F;
constexpr float kLampPeriod = 16.0F;
constexpr std::size_t kLampCount = 9;

constexpr svanes::Color kGroundColor{.red = 54, .green = 48, .blue = 58};
constexpr svanes::Color kRoadColor{.red = 36, .green = 36, .blue = 44};
constexpr svanes::Color kRoadLineColor{.red = 214, .green = 204, .blue = 192};
constexpr svanes::Color kPoleColor{.red = 46, .green = 36, .blue = 40};
constexpr svanes::Color kLampColor{.red = 66, .green = 68, .blue = 78};
constexpr svanes::Color kLampHeadColor{.red = 92, .green = 94, .blue = 104};

/**
 * One upright rectangle of a roadside prop, facing the camera.
 *
 * FIELDS:
 * - left: The left edge, as a sideways distance from the camera.
 * - right: The right edge, as a sideways distance from the camera.
 * - bottom: The bottom edge, as a height above the road.
 * - top: The top edge, as a height above the road.
 * - color: The color the rectangle is drawn in.
 */
struct PropBox {
    float left;
    float right;
    float bottom;
    float top;
    svanes::Color color;
};

constexpr std::array<PropBox, 2> kTelephonePole{{
    {.left = kTelephoneLateral - 0.12F,
     .right = kTelephoneLateral + 0.12F,
     .bottom = 0.0F,
     .top = 4.5F,
     .color = kPoleColor},
    {.left = kTelephoneLateral - 0.8F,
     .right = kTelephoneLateral + 0.8F,
     .bottom = 4.05F,
     .top = 4.2F,
     .color = kPoleColor},
}};

constexpr std::array<PropBox, 3> kStreetLamp{{
    {.left = kLampLateral - 0.08F,
     .right = kLampLateral + 0.08F,
     .bottom = 0.0F,
     .top = 3.6F,
     .color = kLampColor},
    {.left = 0.55F,
     .right = kLampLateral,
     .bottom = 3.5F,
     .top = 3.6F,
     .color = kLampColor},
    {.left = 0.45F,
     .right = 0.95F,
     .bottom = 3.35F,
     .top = 3.52F,
     .color = kLampHeadColor},
}};

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

svanes::Entity CreateMovingShape(svanes::Registry &world, svanes::Color color,
                                 std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(entity,
                                           svanes::SolidShape{.color = color});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

/**
 * Projects a point beside or above the road onto the screen. Distances are
 * measured in camera heights, so a point on the road one camera height ahead
 * lands on the bottom edge of the view.
 *
 * @param lateral The sideways distance from the camera, positive to the right.
 * @param height The height above the road.
 * @param depth The distance ahead of the camera. Must be positive.
 * @return The point's screen position.
 */
svanes::Vector2D ProjectPoint(float lateral, float height, float depth) {
    return {kVanishingX + lateral * kDepthScale / depth,
            kHorizonY + (kCameraHeight - height) * kDepthScale / depth};
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
    return svanes::ConvexPolygon2D{{ProjectPoint(left, 0.0F, far_depth),
                                    ProjectPoint(right, 0.0F, far_depth),
                                    ProjectPoint(right, 0.0F, near_depth),
                                    ProjectPoint(left, 0.0F, near_depth)}};
}

/**
 * Finds the depth of one slot in a row of evenly spaced things that scroll
 * toward the camera.
 *
 * @param index The slot's position in the row, nearest first.
 * @param period The spacing between slots.
 * @param distance How far the camera has driven.
 * @return The slot's distance ahead of the camera.
 */
float SlotDepth(std::size_t index, float period, float distance) {
    const float offset = period - std::fmod(distance, period);
    return kNearestDepth + offset + static_cast<float>(index) * period;
}

svanes::Rectangle2D BoxRectangle(const PropBox &box, float depth) {
    const svanes::Vector2D top_left = ProjectPoint(box.left, box.top, depth);
    const svanes::Vector2D bottom_right =
        ProjectPoint(box.right, box.bottom, depth);
    return {.x = (top_left.x + bottom_right.x) * 0.5F,
            .y = (top_left.y + bottom_right.y) * 0.5F,
            .width = bottom_right.x - top_left.x,
            .height = bottom_right.y - top_left.y};
}

/**
 * Fades a prop in as it comes out of the distance, so the farthest slot of a
 * row does not pop into view when it wraps around.
 *
 * @param depth The prop's distance ahead of the camera.
 * @param row_length The distance covered by the whole row.
 * @return The prop's alpha, 0 at the far end of the row.
 */
std::uint8_t FadeAlpha(float depth, float row_length) {
    const float visible = (kNearestDepth + row_length - depth) / kPropFadeDepth;
    return static_cast<std::uint8_t>(std::clamp(visible, 0.0F, 1.0F) * 255.0F);
}

std::vector<svanes::Entity> CreatePropRow(svanes::Registry &world,
                                          std::span<const PropBox> boxes,
                                          std::size_t count) {
    std::vector<svanes::Entity> entities;
    for (std::size_t slot = 0; slot < count; ++slot) {
        for (const PropBox &box : boxes) {
            entities.push_back(CreateMovingShape(world, box.color, kPropZ));
        }
    }
    return entities;
}

void PlacePropRow(svanes::Registry &world,
                  std::span<const svanes::Entity> entities,
                  std::span<const PropBox> boxes, float period,
                  float distance) {
    const std::size_t count = entities.size() / boxes.size();
    for (std::size_t slot = 0; slot < count; ++slot) {
        const float depth = SlotDepth(slot, period, distance);
        const std::uint8_t alpha =
            FadeAlpha(depth, static_cast<float>(count) * period);
        for (std::size_t part = 0; part < boxes.size(); ++part) {
            svanes::SolidShape &shape = world.GetComponent<svanes::SolidShape>(
                entities[slot * boxes.size() + part]);
            shape.geometry = BoxRectangle(boxes[part], depth);
            shape.color.alpha = alpha;
        }
    }
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
        dashes.push_back(CreateMovingShape(world, kRoadLineColor, kRoadLineZ));
    }
    telephone_poles = CreatePropRow(world, kTelephonePole, kTelephoneCount);
    street_lamps = CreatePropRow(world, kStreetLamp, kLampCount);

    Update(world, 0.0F);
}

void Road::Update(svanes::Registry &world, float seconds) {
    distance = std::fmod(distance + kDriveSpeed * seconds, kPatternLength);

    for (std::size_t index = 0; index < dashes.size(); ++index) {
        const float near_depth = SlotDepth(index, kDashPeriod, distance);
        world.GetComponent<svanes::SolidShape>(dashes[index]).geometry =
            RoadStrip(kRoadCenter - kDashHalfWidth,
                      kRoadCenter + kDashHalfWidth, near_depth,
                      near_depth + kDashLength);
    }
    PlacePropRow(world, telephone_poles, kTelephonePole, kTelephonePeriod,
                 distance);
    PlacePropRow(world, street_lamps, kStreetLamp, kLampPeriod, distance);
}

} // namespace grindset

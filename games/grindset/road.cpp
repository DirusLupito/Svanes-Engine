#include "road.hpp"

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

namespace grindset {

namespace {

constexpr float kViewWidth = 1920.0F;
constexpr float kHorizonY = 600.0F;
constexpr std::int32_t kSkyBandCount = 60;
constexpr float kShiftMargin = 8.0F;
constexpr float kBobSlowPeriod = 2.3F;
constexpr float kBobSlowAmplitude = 2.2F;
constexpr float kBobFastPeriod = 0.9F;
constexpr float kBobFastAmplitude = 0.8F;

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

constexpr std::array<float, 3> kWireOffsets{-0.7F, -0.3F, 0.7F};
constexpr float kWireHeight = 4.2F;
constexpr float kWireSag = 0.6F;
constexpr float kWireThickness = 0.025F;
constexpr std::size_t kWireSegments = 8;
constexpr std::size_t kWireCount =
    kTelephoneCount * kWireOffsets.size() * kWireSegments;

constexpr svanes::Color kGroundColor{.red = 54, .green = 48, .blue = 58};
constexpr svanes::Color kRoadColor{.red = 36, .green = 36, .blue = 44};
constexpr svanes::Color kRoadLineColor{.red = 214, .green = 204, .blue = 192};
constexpr svanes::Color kPoleColor{.red = 46, .green = 36, .blue = 40};
constexpr svanes::Color kLampColor{.red = 66, .green = 68, .blue = 78};
constexpr svanes::Color kLampHeadColor{.red = 92, .green = 94, .blue = 104};
constexpr svanes::Color kWireColor{.red = 30, .green = 26, .blue = 32};

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

std::vector<svanes::Entity> CreateSky(svanes::Registry &world) {
    std::vector<svanes::Entity> bands;
    const float band_height = kHorizonY / static_cast<float>(kSkyBandCount);
    for (std::int32_t band = 0; band < kSkyBandCount; ++band) {
        float center_y = band_height * (static_cast<float>(band) + 0.5F);
        float height = band_height + 1.0F;
        if (band == 0) {
            center_y -= kShiftMargin * 0.5F;
            height += kShiftMargin;
        }
        const float position = (static_cast<float>(band) + 0.5F) /
                               static_cast<float>(kSkyBandCount);

        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(
            entity, svanes::Transform{.x = kViewWidth * 0.5F, .y = center_y});
        world.AddComponent<svanes::SolidShape>(
            entity,
            svanes::SolidShape{.color = SkyColorAt(position),
                               .geometry = svanes::Rectangle2D{
                                   .width = kViewWidth, .height = height}});
        world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kSkyZ});
        bands.push_back(entity);
    }
    return bands;
}

std::array<svanes::Entity, 2> CreateSun(svanes::Registry &world) {
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
    return {glow, sun};
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

float WireHalfThickness(float depth) {
    return std::max(kWireThickness * kDepthScale / depth, 1.0F) * 0.5F;
}

/**
 * Builds one short piece of a wire hanging between two telephone poles. The
 * wire sags lowest halfway between the poles.
 *
 * @param lateral The wire's sideways distance from the camera.
 * @param near_depth The distance ahead of the camera to the nearer pole.
 * @param far_depth The distance ahead of the camera to the farther pole.
 * @param start Where the piece starts, from 0 at the nearer pole to 1 at the
 * farther pole.
 * @param end Where the piece ends, on the same scale as start.
 * @return The piece as a thin parallelogram.
 */
svanes::ConvexPolygon2D WirePiece(float lateral, float near_depth,
                                  float far_depth, float start, float end) {
    const float start_depth = std::lerp(near_depth, far_depth, start);
    const float end_depth = std::lerp(near_depth, far_depth, end);
    const svanes::Vector2D first = ProjectPoint(
        lateral, kWireHeight - kWireSag * 4.0F * start * (1.0F - start),
        start_depth);
    const svanes::Vector2D second = ProjectPoint(
        lateral, kWireHeight - kWireSag * 4.0F * end * (1.0F - end), end_depth);
    const float first_half = WireHalfThickness(start_depth);
    const float second_half = WireHalfThickness(end_depth);
    return svanes::ConvexPolygon2D{{{first.x, first.y - first_half},
                                    {second.x, second.y - second_half},
                                    {second.x, second.y + second_half},
                                    {first.x, first.y + first_half}}};
}

void PlaceWires(svanes::Registry &world,
                std::span<const svanes::Entity> entities, float distance) {
    const float row_length =
        static_cast<float>(kTelephoneCount) * kTelephonePeriod;
    std::size_t next = 0;
    for (std::size_t gap = 0; gap < kTelephoneCount; ++gap) {
        const float far_depth = SlotDepth(gap, kTelephonePeriod, distance);
        const float near_depth = far_depth - kTelephonePeriod;
        const float visible_start =
            std::max((kNearestDepth - near_depth) / kTelephonePeriod, 0.0F);
        for (const float offset : kWireOffsets) {
            for (std::size_t segment = 0; segment < kWireSegments; ++segment) {
                const float start =
                    std::lerp(visible_start, 1.0F,
                              static_cast<float>(segment) /
                                  static_cast<float>(kWireSegments));
                const float end =
                    std::lerp(visible_start, 1.0F,
                              static_cast<float>(segment + 1) /
                                  static_cast<float>(kWireSegments));
                svanes::SolidShape &shape =
                    world.GetComponent<svanes::SolidShape>(entities[next]);
                shape.geometry = WirePiece(kTelephoneLateral + offset,
                                           near_depth, far_depth, start, end);
                shape.color.alpha = FadeAlpha(
                    std::lerp(near_depth, far_depth, end), row_length);
                ++next;
            }
        }
    }
}

/**
 * Finds how far the camera has bobbed up or down. Two slow waves of different
 * lengths are added together so the motion does not repeat in an obvious way.
 *
 * @param elapsed The time since the road was created, in seconds.
 * @return The vertical shift for the road view, in pixels, positive downward.
 */
float BobOffset(float elapsed) {
    constexpr float kTau = 2.0F * std::numbers::pi_v<float>;
    return kBobSlowAmplitude * std::sin(kTau * elapsed / kBobSlowPeriod) +
           kBobFastAmplitude * std::sin(kTau * elapsed / kBobFastPeriod);
}

svanes::Entity CreateGround(svanes::Registry &world) {
    const float bottom = kViewHeight + kShiftMargin;
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = kViewWidth * 0.5F,
                                  .y = (kHorizonY + bottom) * 0.5F});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{
                    .color = kGroundColor,
                    .geometry = svanes::Rectangle2D{
                        .width = kViewWidth, .height = bottom - kHorizonY}});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kGroundZ});
    return entity;
}

svanes::Entity CreateStrip(svanes::Registry &world, float left, float right,
                           svanes::Color color, std::int32_t z_order) {
    const svanes::ConvexPolygon2D shape =
        RoadStrip(left, right, kNearestDepth, kHorizonDepth);
    return CreatePolygon(world, shape.Vertices(), color, z_order);
}

std::array<svanes::Entity, 3> CreateRoad(svanes::Registry &world) {
    const float left = kRoadCenter - kRoadHalfWidth;
    const float right = kRoadCenter + kRoadHalfWidth;
    return {CreateStrip(world, left, right, kRoadColor, kRoadZ),
            CreateStrip(world, left + kEdgeLineInset,
                        left + kEdgeLineInset + kEdgeLineWidth, kRoadLineColor,
                        kRoadLineZ),
            CreateStrip(world, right - kEdgeLineInset - kEdgeLineWidth,
                        right - kEdgeLineInset, kRoadLineColor, kRoadLineZ)};
}

} // namespace

void Road::Initialize(svanes::Registry &world) {
    std::vector<svanes::Entity> created = CreateSky(world);
    const std::array<svanes::Entity, 2> sun = CreateSun(world);
    created.insert(created.end(), sun.begin(), sun.end());
    created.push_back(CreateGround(world));
    const std::array<svanes::Entity, 3> road = CreateRoad(world);
    created.insert(created.end(), road.begin(), road.end());

    for (std::size_t index = 0; index < kDashCount; ++index) {
        dashes.push_back(CreateMovingShape(world, kRoadLineColor, kRoadLineZ));
    }
    telephone_poles = CreatePropRow(world, kTelephonePole, kTelephoneCount);
    street_lamps = CreatePropRow(world, kStreetLamp, kLampCount);
    for (std::size_t index = 0; index < kWireCount; ++index) {
        wires.push_back(CreateMovingShape(world, kWireColor, kPropZ));
    }
    created.insert(created.end(), dashes.begin(), dashes.end());
    created.insert(created.end(), telephone_poles.begin(),
                   telephone_poles.end());
    created.insert(created.end(), street_lamps.begin(), street_lamps.end());
    created.insert(created.end(), wires.begin(), wires.end());

    for (const svanes::Entity entity : created) {
        shapes.push_back(
            {.entity = entity,
             .resting_y = world.GetComponent<svanes::Transform>(entity).y});
    }

    Update(world, 0.0F);
}

void Road::ShiftVertically(svanes::Registry &world, float offset) {
    for (const RestingShape &shape : shapes) {
        world.GetComponent<svanes::Transform>(shape.entity).y =
            shape.resting_y + offset;
    }
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
    PlaceWires(world, wires, distance);
    elapsed += seconds;
    ShiftVertically(world, BobOffset(elapsed));
}

} // namespace grindset

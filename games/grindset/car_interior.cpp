#include "car_interior.hpp"

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

namespace grindset {

namespace {

constexpr float kViewWidth = 1920.0F;
constexpr float kViewHeight = 1080.0F;
constexpr float kMargin = 12.0F;
constexpr float kBobShare = 0.4F;

constexpr float kRoofBottom = 70.0F;

constexpr float kDashEdgeY = 872.0F;
constexpr float kDashArch = 36.0F;
constexpr float kHoodRaise = 24.0F;
constexpr float kHoodInset = 60.0F;
constexpr std::int32_t kArchSegments = 16;

constexpr svanes::Vector2D kWheelCenter{960.0F, 1180.0F};
constexpr float kWheelRadius = 400.0F;
constexpr float kWheelThickness = 44.0F;
constexpr float kWheelStartDegrees = -195.0F;
constexpr float kWheelEndDegrees = 15.0F;
constexpr std::int32_t kWheelSegments = 28;

constexpr svanes::Rectangle2D kMirrorStem{
    .x = 1250.0F, .y = 86.0F, .width = 18.0F, .height = 52.0F};
constexpr svanes::Rectangle2D kMirrorFrame{
    .x = 1250.0F, .y = 180.0F, .width = 460.0F, .height = 140.0F};
constexpr float kMirrorFrameWidth = 10.0F;

constexpr std::int32_t kHoodZ = 50;
constexpr std::int32_t kDashZ = 51;
constexpr std::int32_t kFrameZ = 52;
constexpr std::int32_t kWheelZ = 53;
constexpr std::int32_t kMirrorZ = 54;
constexpr std::int32_t kMirrorFaceZ = 55;

constexpr svanes::Color kFrameColor{.red = 24, .green = 22, .blue = 28};
constexpr svanes::Color kDashColor{.red = 32, .green = 30, .blue = 36};
constexpr svanes::Color kHoodColor{.red = 70, .green = 74, .blue = 90};
constexpr svanes::Color kWheelColor{.red = 18, .green = 17, .blue = 21};
constexpr svanes::Color kMirrorFaceColor{.red = 92, .green = 96, .blue = 108};

svanes::Entity CreatePart(svanes::Registry &world, svanes::SolidShape shape,
                          std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(entity, std::move(shape));
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

/**
 * Builds a shape with a straight bottom edge and a top edge that arches
 * upward in the middle.
 *
 * @param left The left edge.
 * @param right The right edge.
 * @param edge_y The height of the top edge at the left and right ends.
 * @param arch How much higher the top edge is in the middle than at the ends.
 * @param bottom The bottom edge.
 * @return The arched shape.
 */
svanes::ConvexPolygon2D ArchedShape(float left, float right, float edge_y,
                                    float arch, float bottom) {
    std::vector<svanes::Vector2D> vertices{{left, bottom}, {right, bottom}};
    for (std::int32_t step = kArchSegments; step >= 0; --step) {
        const float amount =
            static_cast<float>(step) / static_cast<float>(kArchSegments);
        vertices.push_back(
            {std::lerp(left, right, amount),
             edge_y - arch * std::sin(std::numbers::pi_v<float> * amount)});
    }
    return svanes::ConvexPolygon2D{std::move(vertices)};
}

/**
 * Builds one short piece of the steering wheel's rim.
 *
 * @param start_degrees Where the piece starts around the wheel.
 * @param end_degrees Where the piece ends around the wheel.
 * @return The piece as a four sided shape between the rim's inner and outer
 * edges.
 */
svanes::ConvexPolygon2D WheelPiece(float start_degrees, float end_degrees) {
    const float to_radians = std::numbers::pi_v<float> / 180.0F;
    const float start = start_degrees * to_radians;
    const float end = end_degrees * to_radians;
    const float inner = kWheelRadius - kWheelThickness;
    const auto point = [](float radius, float angle) {
        return svanes::Vector2D{kWheelCenter.x + radius * std::cos(angle),
                                kWheelCenter.y + radius * std::sin(angle)};
    };
    return svanes::ConvexPolygon2D{{point(kWheelRadius, start),
                                    point(kWheelRadius, end), point(inner, end),
                                    point(inner, start)}};
}

void CreateWindshieldFrame(svanes::Registry &world,
                           std::vector<svanes::Entity> &parts) {
    parts.push_back(CreatePart(
        world,
        svanes::SolidShape{
            .color = kFrameColor,
            .geometry =
                svanes::Rectangle2D{.x = kViewWidth * 0.5F,
                                    .y = (kRoofBottom - kMargin) * 0.5F,
                                    .width = kViewWidth + kMargin * 2.0F,
                                    .height = kRoofBottom + kMargin}},
        kFrameZ));
    parts.push_back(
        CreatePart(world,
                   svanes::SolidShape{.color = kFrameColor,
                                      .geometry =
                                          svanes::ConvexPolygon2D{
                                              {{-kMargin, kRoofBottom - 10.0F},
                                               {100.0F, kRoofBottom - 10.0F},
                                               {480.0F, kDashEdgeY + 10.0F},
                                               {270.0F, kDashEdgeY + 10.0F}}}},
                   kFrameZ));
}

void CreateDashboard(svanes::Registry &world,
                     std::vector<svanes::Entity> &parts) {
    parts.push_back(CreatePart(
        world,
        svanes::SolidShape{.color = kHoodColor,
                           .geometry = ArchedShape(
                               kHoodInset, kViewWidth - kHoodInset,
                               kDashEdgeY - kHoodRaise * 0.25F,
                               kDashArch + kHoodRaise, kDashEdgeY + 40.0F)},
        kHoodZ));
    parts.push_back(CreatePart(
        world,
        svanes::SolidShape{
            .color = kDashColor,
            .geometry = ArchedShape(-kMargin, kViewWidth + kMargin, kDashEdgeY,
                                    kDashArch, kViewHeight + kMargin)},
        kDashZ));
}

void CreateSteeringWheel(svanes::Registry &world,
                         std::vector<svanes::Entity> &parts) {
    const float step = (kWheelEndDegrees - kWheelStartDegrees) /
                       static_cast<float>(kWheelSegments);
    for (std::int32_t segment = 0; segment < kWheelSegments; ++segment) {
        const float start =
            kWheelStartDegrees + step * static_cast<float>(segment);
        parts.push_back(CreatePart(
            world,
            svanes::SolidShape{.color = kWheelColor,
                               .geometry = WheelPiece(start, start + step)},
            kWheelZ));
    }
}

void CreateMirror(svanes::Registry &world, std::vector<svanes::Entity> &parts) {
    parts.push_back(CreatePart(
        world,
        svanes::SolidShape{.color = kFrameColor, .geometry = kMirrorStem},
        kMirrorZ));
    parts.push_back(CreatePart(
        world,
        svanes::SolidShape{.color = kFrameColor, .geometry = kMirrorFrame},
        kMirrorZ));
    parts.push_back(CreatePart(
        world,
        svanes::SolidShape{
            .color = kMirrorFaceColor,
            .geometry =
                svanes::Rectangle2D{
                    .x = kMirrorFrame.x,
                    .y = kMirrorFrame.y,
                    .width = kMirrorFrame.width - kMirrorFrameWidth * 2.0F,
                    .height = kMirrorFrame.height - kMirrorFrameWidth * 2.0F}},
        kMirrorFaceZ));
}

} // namespace

void CarInterior::Initialize(svanes::Registry &world) {
    CreateWindshieldFrame(world, parts);
    CreateDashboard(world, parts);
    CreateSteeringWheel(world, parts);
    CreateMirror(world, parts);
}

void CarInterior::Update(svanes::Registry &world, float road_bob) {
    for (const svanes::Entity entity : parts) {
        world.GetComponent<svanes::Transform>(entity).y = road_bob * kBobShare;
    }
}

} // namespace grindset

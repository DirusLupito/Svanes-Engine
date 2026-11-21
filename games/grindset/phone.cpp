#include "phone.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace grindset {

namespace {

constexpr float kBodyWidth = 422.0F;
constexpr float kBodyHeight = 880.0F;
constexpr float kBodyBottom = 1040.0F;
constexpr svanes::Vector2D kHeldPosition{1400.0F,
                                         kBodyBottom - kBodyHeight * 0.5F};
constexpr float kViewHeight = 1080.0F;
constexpr float kPocketGap = 20.0F;
constexpr float kPocketY = kViewHeight + kBodyHeight * 0.5F + kPocketGap;
constexpr float kSlideSeconds = 0.35F;

constexpr float kSwaySidewaysPeriod = 3.7F;
constexpr float kSwaySidewaysAmplitude = 5.0F;
constexpr float kSwayVerticalPeriod = 2.6F;
constexpr float kSwayVerticalAmplitude = 4.0F;

constexpr float kBodyCornerRadius = 50.0F;
constexpr float kBezel = 14.0F;
constexpr float kScreenWidth = kBodyWidth - kBezel * 2.0F;
constexpr float kScreenHeight = kBodyHeight - kBezel * 2.0F;
constexpr float kScreenCornerRadius = kBodyCornerRadius - kBezel;
constexpr float kCameraRadius = 9.0F;
constexpr float kCameraFromTop = 26.0F;
constexpr std::int32_t kCornerSegments = 8;

constexpr std::int32_t kBodyZ = 100;
constexpr std::int32_t kScreenZ = 101;
constexpr std::int32_t kCameraZ = 102;
constexpr std::int32_t kHintZ = 103;
constexpr std::int32_t kStatusZ = 103;

constexpr float kStatusInset = 30.0F;
constexpr float kStatusFromTop = 14.0F;
constexpr svanes::Color kStatusColor{.red = 240, .green = 240, .blue = 244};

constexpr svanes::Vector2D kHintPosition{1400.0F, 1010.0F};
constexpr svanes::Color kHintColor{.red = 232, .green = 226, .blue = 214};

constexpr svanes::Color kBodyColor{.red = 20, .green = 20, .blue = 24};
constexpr svanes::Color kScreenColor{.red = 44, .green = 52, .blue = 84};
constexpr svanes::Color kCameraColor{.red = 6, .green = 6, .blue = 8};

/**
 * Builds a rectangle with rounded corners centered on the origin, as a convex
 * polygon with a few points around each corner.
 *
 * @param width The full width.
 * @param height The full height.
 * @param radius The radius of each corner.
 * @return The rounded rectangle.
 */
svanes::ConvexPolygon2D RoundedRectangle(float width, float height,
                                         float radius) {
    const float inner_x = width * 0.5F - radius;
    const float inner_y = height * 0.5F - radius;
    const float quarter_turn = std::numbers::pi_v<float> * 0.5F;
    const std::array<svanes::Vector2D, 4> corner_centers{{
        {inner_x, inner_y},
        {-inner_x, inner_y},
        {-inner_x, -inner_y},
        {inner_x, -inner_y},
    }};

    std::vector<svanes::Vector2D> vertices;
    for (std::size_t corner = 0; corner < corner_centers.size(); ++corner) {
        for (std::int32_t step = 0; step <= kCornerSegments; ++step) {
            const float angle =
                quarter_turn * (static_cast<float>(corner) +
                                static_cast<float>(step) /
                                    static_cast<float>(kCornerSegments));
            vertices.push_back(
                {corner_centers[corner].x + radius * std::cos(angle),
                 corner_centers[corner].y + radius * std::sin(angle)});
        }
    }
    return svanes::ConvexPolygon2D{std::move(vertices)};
}

svanes::Entity CreatePart(svanes::Registry &world, svanes::Vector2D position,
                          svanes::SolidShape shape, std::int32_t z_order) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        entity, svanes::Transform{.x = position.x, .y = position.y});
    world.AddComponent<svanes::SolidShape>(entity, std::move(shape));
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

/**
 * Finds how far an unsteady hand has moved the held phone from where it rests.
 * The sideways and up-down waves have different lengths so the motion does
 * not repeat in an obvious way.
 *
 * @param elapsed The time since the phone was created, in seconds.
 * @return The shift from the held position, in pixels.
 */
svanes::Vector2D SwayOffset(float elapsed) {
    constexpr float kTau = 2.0F * std::numbers::pi_v<float>;
    return {kSwaySidewaysAmplitude *
                std::sin(kTau * elapsed / kSwaySidewaysPeriod),
            kSwayVerticalAmplitude *
                std::sin(kTau * elapsed / kSwayVerticalPeriod)};
}

std::string FormatDollars(std::int64_t cents) {
    if (cents < 0) {
        throw std::invalid_argument("Phone cannot show a negative balance of " +
                                    std::to_string(cents) + " cents.");
    }
    return std::format("${}.{:02}", cents / 100, cents % 100);
}

svanes::Entity CreateStatusLabel(svanes::Registry &world, std::string text,
                                 svanes::FontHandle font,
                                 svanes::TextAlignment alignment) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        entity, svanes::TextLabel{.text = std::move(text),
                                  .color = kStatusColor,
                                  .font = font,
                                  .alignment = alignment});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kStatusZ});
    return entity;
}

} // namespace

void Phone::Initialize(svanes::Registry &world, svanes::FontHandle hint_font,
                       svanes::FontHandle status_font) {
    parts.push_back(CreatePart(
        world, position,
        svanes::SolidShape{.color = kBodyColor,
                           .geometry = RoundedRectangle(kBodyWidth, kBodyHeight,
                                                        kBodyCornerRadius)},
        kBodyZ));
    parts.push_back(
        CreatePart(world, position,
                   svanes::SolidShape{
                       .color = kScreenColor,
                       .geometry = RoundedRectangle(kScreenWidth, kScreenHeight,
                                                    kScreenCornerRadius)},
                   kScreenZ));
    parts.push_back(CreatePart(
        world, position,
        svanes::SolidShape{
            .color = kCameraColor,
            .geometry =
                svanes::Circle2D{.y = -kScreenHeight * 0.5F + kCameraFromTop,
                                 .radius = kCameraRadius}},
        kCameraZ));
    clock = CreateStatusLabel(world, "5:30am", status_font,
                              svanes::TextAlignment::TopLeft);
    balance = CreateStatusLabel(world, FormatDollars(0), status_font,
                                svanes::TextAlignment::TopRight);
    Place(world);

    hint = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        hint,
        svanes::TextLabel{.text = "Press Space to take out your phone",
                          .position = kHintPosition,
                          .color = kHintColor,
                          .font = hint_font,
                          .alignment = svanes::TextAlignment::BottomCenter});
    world.AddComponent<svanes::ZOrder>(hint, svanes::ZOrder{kHintZ});
}

void Phone::Toggle() { held = !held; }

void Phone::Update(svanes::Registry &world, float seconds,
                   std::int64_t balance_cents) {
    elapsed += seconds;
    const float step = seconds / kSlideSeconds;
    if (held) {
        slide = std::min(slide + step, 1.0F);
    } else {
        slide = std::max(slide - step, 0.0F);
    }
    Place(world);
    world.GetComponent<svanes::TextLabel>(hint).visible = slide == 0.0F;
    world.GetComponent<svanes::TextLabel>(balance).text =
        FormatDollars(balance_cents);
}

void Phone::Place(svanes::Registry &world) {
    const float eased = slide * slide * (3.0F - 2.0F * slide);
    const svanes::Vector2D sway = SwayOffset(elapsed);
    position = {kHeldPosition.x + sway.x * eased,
                std::lerp(kPocketY, kHeldPosition.y, eased) + sway.y * eased};
    for (const svanes::Entity entity : parts) {
        svanes::Transform &transform =
            world.GetComponent<svanes::Transform>(entity);
        transform.x = position.x;
        transform.y = position.y;
    }

    const svanes::Rectangle2D screen = ScreenRectangle();
    const float top = screen.y - screen.height * 0.5F + kStatusFromTop;
    world.GetComponent<svanes::TextLabel>(clock).position = {
        screen.x - screen.width * 0.5F + kStatusInset, top};
    world.GetComponent<svanes::TextLabel>(balance).position = {
        screen.x + screen.width * 0.5F - kStatusInset, top};
}

svanes::Rectangle2D Phone::ScreenRectangle() const {
    return {.x = position.x,
            .y = position.y,
            .width = kScreenWidth,
            .height = kScreenHeight};
}

} // namespace grindset

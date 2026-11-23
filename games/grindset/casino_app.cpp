#include "casino_app.hpp"

#include "phone.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <cstdint>
#include <utility>

namespace grindset {

namespace {

constexpr float kStreamTop = 44.0F;
constexpr float kStreamHeight = 320.0F;

constexpr float kDealerHeadRadius = 34.0F;
constexpr float kDealerHeadFromTop = 120.0F;
constexpr float kDealerBodyWidth = 170.0F;
constexpr float kDealerBodyHeight = 120.0F;
constexpr float kDealerBodyFromTop = 215.0F;
constexpr float kDealerCollarWidth = 30.0F;
constexpr float kDealerCollarHeight = 40.0F;

constexpr float kTableHeight = 70.0F;
constexpr float kRailHeight = 10.0F;

constexpr float kBadgeWidth = 58.0F;
constexpr float kBadgeHeight = 26.0F;
constexpr float kBadgeInset = 16.0F;

constexpr std::int32_t kStreamZ = Phone::kAppZ;
constexpr std::int32_t kDealerBodyZ = Phone::kAppZ + 1;
constexpr std::int32_t kDealerCollarZ = Phone::kAppZ + 2;
constexpr std::int32_t kDealerHeadZ = Phone::kAppZ + 3;
constexpr std::int32_t kTableZ = Phone::kAppZ + 4;
constexpr std::int32_t kRailZ = Phone::kAppZ + 5;
constexpr std::int32_t kBadgeZ = Phone::kAppZ + 6;

constexpr svanes::Color kStreamColor{.red = 14, .green = 14, .blue = 20};
constexpr svanes::Color kDealerHeadColor{.red = 206, .green = 170, .blue = 140};
constexpr svanes::Color kDealerBodyColor{.red = 30, .green = 30, .blue = 38};
constexpr svanes::Color kDealerCollarColor{
    .red = 230, .green = 230, .blue = 235};
constexpr svanes::Color kTableColor{.red = 20, .green = 92, .blue = 56};
constexpr svanes::Color kRailColor{.red = 86, .green = 52, .blue = 30};
constexpr svanes::Color kBadgeColor{.red = 214, .green = 40, .blue = 52};
constexpr svanes::Color kBadgeTextColor{.red = 255, .green = 255, .blue = 255};

/**
 * Fades an alpha by how much of the app is showing.
 *
 * @param alpha The alpha when the app is fully shown.
 * @param shown How much of the app is showing, from 0 to 1.
 * @return The faded alpha.
 */
std::uint8_t Faded(std::uint8_t alpha, float shown) {
    return static_cast<std::uint8_t>(static_cast<float>(alpha) * shown);
}

} // namespace

void CasinoApp::Initialize(svanes::Registry &world, svanes::FontHandle font,
                           svanes::Rectangle2D screen) {
    const float top = -screen.height * 0.5F + kStreamTop;
    const float stream_bottom = top + kStreamHeight;
    const auto add = [&](svanes::Color color, svanes::Geometry2D geometry,
                         std::int32_t z_order) {
        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
        world.AddComponent<svanes::SolidShape>(
            entity, svanes::SolidShape{.color = {.alpha = 0},
                                       .geometry = std::move(geometry)});
        world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
        world.AddComponent<svanes::ClipRect>(entity, svanes::ClipRect{});
        parts.push_back({.entity = entity, .color = color});
    };

    add(kStreamColor,
        svanes::Rectangle2D{.y = top + kStreamHeight * 0.5F,
                            .width = screen.width,
                            .height = kStreamHeight},
        kStreamZ);
    add(kDealerBodyColor,
        svanes::Rectangle2D{.y = top + kDealerBodyFromTop,
                            .width = kDealerBodyWidth,
                            .height = kDealerBodyHeight},
        kDealerBodyZ);
    add(kDealerCollarColor,
        svanes::Rectangle2D{.y = top + kDealerBodyFromTop -
                                 (kDealerBodyHeight - kDealerCollarHeight) *
                                     0.5F,
                            .width = kDealerCollarWidth,
                            .height = kDealerCollarHeight},
        kDealerCollarZ);
    add(kDealerHeadColor,
        svanes::Circle2D{.y = top + kDealerHeadFromTop,
                         .radius = kDealerHeadRadius},
        kDealerHeadZ);
    add(kTableColor,
        svanes::Rectangle2D{.y = stream_bottom - kTableHeight * 0.5F,
                            .width = screen.width,
                            .height = kTableHeight},
        kTableZ);
    add(kRailColor,
        svanes::Rectangle2D{.y = stream_bottom - kTableHeight +
                                 kRailHeight * 0.5F,
                            .width = screen.width,
                            .height = kRailHeight},
        kRailZ);
    add(kBadgeColor,
        svanes::Rectangle2D{.x = -screen.width * 0.5F + kBadgeInset +
                                 kBadgeWidth * 0.5F,
                            .y = top + kBadgeInset + kBadgeHeight * 0.5F,
                            .width = kBadgeWidth,
                            .height = kBadgeHeight},
        kBadgeZ);

    live_label = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        live_label,
        svanes::TextLabel{.text = "LIVE",
                          .font = font,
                          .alignment = svanes::TextAlignment::Center,
                          .visible = false});
    world.AddComponent<svanes::ZOrder>(live_label, svanes::ZOrder{kBadgeZ});
    world.AddComponent<svanes::ClipRect>(live_label, svanes::ClipRect{});
}

void CasinoApp::Update(svanes::Registry &world, svanes::Rectangle2D screen,
                       svanes::Rectangle2D area, float shown) {
    for (const Part &part : parts) {
        svanes::Transform &transform =
            world.GetComponent<svanes::Transform>(part.entity);
        transform.x = screen.x;
        transform.y = screen.y;
        svanes::Color color = part.color;
        color.alpha = Faded(color.alpha, shown);
        world.GetComponent<svanes::SolidShape>(part.entity).color = color;
        world.GetComponent<svanes::ClipRect>(part.entity).area = area;
    }

    svanes::TextLabel &label =
        world.GetComponent<svanes::TextLabel>(live_label);
    label.position = {screen.x - screen.width * 0.5F + kBadgeInset +
                          kBadgeWidth * 0.5F,
                      screen.y - screen.height * 0.5F + kStreamTop +
                          kBadgeInset + kBadgeHeight * 0.5F};
    label.color = kBadgeTextColor;
    label.color.alpha = Faded(kBadgeTextColor.alpha, shown);
    label.visible = shown > 0.0F;
    world.GetComponent<svanes::ClipRect>(live_label).area = area;
}

} // namespace grindset

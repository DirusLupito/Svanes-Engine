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
constexpr std::int32_t kWallpaperCapZ = 102;
constexpr std::int32_t kWallpaperBandZ = 103;
constexpr std::int32_t kIconZ = 104;
constexpr std::int32_t kAppCoverZ = 105;
constexpr std::int32_t kHomeBarZ = Phone::kAppZ + 10;
constexpr std::int32_t kCameraZ = Phone::kAppZ + 11;
constexpr std::int32_t kHintZ = Phone::kAppZ + 12;
constexpr std::int32_t kStatusZ = Phone::kAppZ + 12;

constexpr float kAppGrowSeconds = 0.3F;
constexpr float kAppFadeSeconds = 0.25F;
constexpr float kAppContentShownAbove = 0.8F;

constexpr float kHomeBarWidth = 134.0F;
constexpr float kHomeBarHeight = 7.0F;
constexpr float kHomeBarRadius = 3.0F;
constexpr float kHomeBarFromBottom = 14.0F;
constexpr svanes::Color kHomeBarColor{
    .red = 255, .green = 255, .blue = 255, .alpha = 210};

constexpr float kSwipeZoneHeight = 70.0F;
constexpr float kSwipeSampleSeconds = 0.06F;
constexpr float kCloseFraction = 0.75F;
constexpr float kCloseLookaheadSeconds = 0.5F;
constexpr float kSpringStiffness = 300.0F;
constexpr float kSpringDamping = 35.0F;
constexpr float kSpringRestDistance = 0.001F;
constexpr float kSpringRestSpeed = 0.01F;

constexpr std::int32_t kWallpaperBandCount = 40;
constexpr float kWallpaperCapExtra = 20.0F;
constexpr svanes::Color kWallpaperTop{.red = 46, .green = 58, .blue = 112};
constexpr svanes::Color kWallpaperBottom{.red = 148, .green = 92, .blue = 138};

constexpr float kIconColumns = 3.0F;
constexpr float kIconColumnWidth = kScreenWidth / kIconColumns;
constexpr float kIconSize = 80.0F;
constexpr float kIconCornerRadius = 18.0F;
constexpr float kIconFromTop = 100.0F;
constexpr float kIconNameGap = 8.0F;
constexpr float kPressedShade = 0.7F;

constexpr float kStatusInset = 30.0F;
constexpr float kStatusFromTop = 14.0F;
constexpr svanes::Color kStatusColor{.red = 240, .green = 240, .blue = 244};

/**
 * One app on the home screen.
 *
 * FIELDS:
 * - name: The name written under the icon.
 * - color: The icon's color.
 */
struct AppIcon {
    const char *name;
    svanes::Color color;
};

constexpr std::array<AppIcon, 2> kAppIcons{{
    {.name = "Casino", .color = {.red = 34, .green = 124, .blue = 74}},
    {.name = "Instagram", .color = {.red = 222, .green = 84, .blue = 118}},
}};

constexpr svanes::Vector2D kHintPosition{1400.0F, 1010.0F};
constexpr svanes::Color kHintColor{.red = 232, .green = 226, .blue = 214};

constexpr svanes::Color kBodyColor{.red = 20, .green = 20, .blue = 24};
constexpr svanes::Color kCameraColor{.red = 6, .green = 6, .blue = 8};

/**
 * Builds a rectangle with rounded corners, as a convex polygon with a few
 * points around each corner. The straight part of each side must be longer
 * than zero.
 *
 * @param width The full width.
 * @param height The full height.
 * @param radius The radius of each corner.
 * @param center Where the rectangle's center is.
 * @return The rounded rectangle.
 */
svanes::ConvexPolygon2D RoundedRectangle(float width, float height,
                                         float radius,
                                         svanes::Vector2D center = {}) {
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
                {center.x + corner_centers[corner].x + radius * std::cos(angle),
                 center.y + corner_centers[corner].y +
                     radius * std::sin(angle)});
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

std::uint8_t Mix(std::uint8_t from, std::uint8_t to, float amount) {
    const float value =
        static_cast<float>(from) +
        (static_cast<float>(to) - static_cast<float>(from)) * amount;
    return static_cast<std::uint8_t>(value + 0.5F);
}

svanes::Color MixColor(svanes::Color from, svanes::Color to, float amount) {
    return {.red = Mix(from.red, to.red, amount),
            .green = Mix(from.green, to.green, amount),
            .blue = Mix(from.blue, to.blue, amount),
            .alpha = Mix(from.alpha, to.alpha, amount)};
}

float SmoothStep(float amount) {
    return amount * amount * (3.0F - 2.0F * amount);
}

svanes::Color Shade(svanes::Color color, float amount) {
    return MixColor({.alpha = color.alpha}, color, amount);
}

/**
 * Finds where an app icon's center sits relative to the phone's center.
 *
 * @param index The app's column on the home screen, from the left. The
 * screen is split into three equal columns, so the middle column is centered.
 * @return The icon's center, relative to the phone's center.
 */
svanes::Vector2D IconCenter(std::size_t index) {
    return {-kScreenWidth * 0.5F +
                kIconColumnWidth * (static_cast<float>(index) + 0.5F),
            -kScreenHeight * 0.5F + kIconFromTop};
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

std::string FormatDollars(std::int64_t cents) {
    if (cents < 0) {
        throw std::invalid_argument("Phone cannot show a negative balance of " +
                                    std::to_string(cents) + " cents.");
    }
    return std::format("${}.{:02}", cents / 100, cents % 100);
}

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
                       .color = kWallpaperTop,
                       .geometry = RoundedRectangle(kScreenWidth, kScreenHeight,
                                                    kScreenCornerRadius)},
                   kScreenZ));
    CreateWallpaper(world);
    CreateIcons(world, status_font);
    app_cover = CreatePart(world, position,
                           svanes::SolidShape{.color = {.alpha = 0},
                                              .geometry = RoundedRectangle(
                                                  kScreenWidth, kScreenHeight,
                                                  kScreenCornerRadius)},
                           kAppCoverZ);
    parts.push_back(app_cover);
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
    home_bar =
        CreatePart(world, position,
                   svanes::SolidShape{
                       .color = {.alpha = 0},
                       .geometry = RoundedRectangle(
                           kHomeBarWidth, kHomeBarHeight, kHomeBarRadius,
                           {0.0F, kScreenHeight * 0.5F - kHomeBarFromBottom})},
                   kHomeBarZ);
    parts.push_back(home_bar);

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

void Phone::CreateWallpaper(svanes::Registry &world) {
    const float cap_height = kScreenCornerRadius * 2.0F + kWallpaperCapExtra;
    parts.push_back(
        CreatePart(world, position,
                   svanes::SolidShape{
                       .color = kWallpaperBottom,
                       .geometry = RoundedRectangle(
                           kScreenWidth, cap_height, kScreenCornerRadius,
                           {0.0F, kScreenHeight * 0.5F - cap_height * 0.5F})},
                   kWallpaperCapZ));

    const float top = -kScreenHeight * 0.5F + kScreenCornerRadius;
    const float band_height = (kScreenHeight - kScreenCornerRadius * 2.0F) /
                              static_cast<float>(kWallpaperBandCount);
    for (std::int32_t band = 0; band < kWallpaperBandCount; ++band) {
        const float amount = (static_cast<float>(band) + 0.5F) /
                             static_cast<float>(kWallpaperBandCount);
        parts.push_back(CreatePart(
            world, position,
            svanes::SolidShape{
                .color = MixColor(kWallpaperTop, kWallpaperBottom, amount),
                .geometry =
                    svanes::Rectangle2D{
                        .y = top +
                             band_height * (static_cast<float>(band) + 0.5F),
                        .width = kScreenWidth,
                        .height = band_height + 1.0F}},
            kWallpaperBandZ));
    }
}

void Phone::CreateIcons(svanes::Registry &world, svanes::FontHandle font) {
    for (std::size_t index = 0; index < kAppIcons.size(); ++index) {
        icons[index] = CreatePart(
            world, position,
            svanes::SolidShape{.color = kAppIcons[index].color,
                               .geometry = RoundedRectangle(
                                   kIconSize, kIconSize, kIconCornerRadius,
                                   IconCenter(index))},
            kIconZ);
        parts.push_back(icons[index]);
        icon_names[index] =
            CreateStatusLabel(world, kAppIcons[index].name, font,
                              svanes::TextAlignment::TopCenter);
    }
}

void Phone::Toggle() { held = !held; }

void Phone::Update(svanes::Registry &world, const svanes::InputManager &input,
                   float seconds, std::int64_t balance_cents) {
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

    TrackClick(input);
    TrackSwipe(input);
    MoveApp(seconds);
    ShowScreen(world);
}

void Phone::TrackClick(const svanes::InputManager &input) {
    if (input.WasMouseButtonReleased(svanes::MouseButton::Left)) {
        if (pressed_icon.has_value() && slide == 1.0F &&
            IconUnderMouse(input) == pressed_icon) {
            open_app = pressed_icon;
            open_seconds = 0.0F;
            app_motion = AppMotion::Opening;
            app_amount = 0.0F;
        }
        pressed_icon.reset();
        return;
    }
    if (input.WasMouseButtonPressed(svanes::MouseButton::Left) &&
        slide == 1.0F && !open_app.has_value()) {
        pressed_icon = IconUnderMouse(input);
    }
}

std::optional<std::size_t>
Phone::IconUnderMouse(const svanes::InputManager &input) const {
    const svanes::Vector2D mouse = input.MousePosition();
    for (std::size_t index = 0; index < icons.size(); ++index) {
        const svanes::Vector2D center = IconCenter(index);
        const float dx = mouse.x - (position.x + center.x);
        const float dy = mouse.y - (position.y + center.y);
        if (std::abs(dx) <= kIconSize * 0.5F &&
            std::abs(dy) <= kIconSize * 0.5F) {
            return index;
        }
    }
    return std::nullopt;
}

void Phone::TrackSwipe(const svanes::InputManager &input) {
    if (!open_app.has_value()) {
        return;
    }
    const svanes::Vector2D mouse = input.MousePosition();
    if (app_motion == AppMotion::Dragging) {
        swipe_samples.push_back({.seconds = elapsed, .y = mouse.y});
        std::erase_if(swipe_samples, [this](const SwipeSample &sample) {
            return sample.seconds < elapsed - kSwipeSampleSeconds;
        });
        const float dragged = std::max(swipe_start_y - mouse.y, 0.0F);
        app_amount = 1.0F - std::min(dragged / kScreenHeight, 1.0F);
        if (input.IsMouseButtonDown(svanes::MouseButton::Left)) {
            return;
        }

        const SwipeSample &oldest = swipe_samples.front();
        const SwipeSample &newest = swipe_samples.back();
        const float sample_seconds = newest.seconds - oldest.seconds;
        const float upward_speed = sample_seconds > 0.0F
                                       ? (oldest.y - newest.y) / sample_seconds
                                       : 0.0F;
        const float reach = dragged + upward_speed * kCloseLookaheadSeconds;
        app_target = reach >= kCloseFraction * kScreenHeight ? 0.0F : 1.0F;
        app_speed = -upward_speed / kScreenHeight;
        app_motion = AppMotion::Settling;
        return;
    }

    if (app_motion != AppMotion::Open || slide != 1.0F ||
        !input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        return;
    }
    const svanes::Rectangle2D screen = ScreenRectangle();
    const float bottom = screen.y + screen.height * 0.5F;
    if (std::abs(mouse.x - screen.x) <= screen.width * 0.5F &&
        mouse.y <= bottom && mouse.y >= bottom - kSwipeZoneHeight) {
        app_motion = AppMotion::Dragging;
        swipe_start_y = mouse.y;
        swipe_samples = {{.seconds = elapsed, .y = mouse.y}};
    }
}

void Phone::MoveApp(float seconds) {
    if (!open_app.has_value()) {
        return;
    }
    if (app_motion == AppMotion::Opening) {
        open_seconds += seconds;
        app_amount = SmoothStep(std::min(open_seconds / kAppGrowSeconds, 1.0F));
        if (open_seconds >= kAppGrowSeconds + kAppFadeSeconds) {
            app_motion = AppMotion::Open;
        }
        return;
    }
    if (app_motion != AppMotion::Settling) {
        return;
    }

    const float pull = kSpringStiffness * (app_target - app_amount) -
                       kSpringDamping * app_speed;
    app_speed += pull * seconds;
    app_amount += app_speed * seconds;
    const bool passed_target =
        app_target == 0.0F ? app_amount <= 0.0F : app_amount >= 1.0F;
    const bool at_rest =
        std::abs(app_target - app_amount) < kSpringRestDistance &&
        std::abs(app_speed) < kSpringRestSpeed;
    if (!passed_target && !at_rest) {
        return;
    }
    app_amount = app_target;
    app_speed = 0.0F;
    if (app_target == 0.0F) {
        open_app.reset();
    } else {
        app_motion = AppMotion::Open;
    }
}

void Phone::ShowScreen(svanes::Registry &world) {
    for (std::size_t index = 0; index < icons.size(); ++index) {
        svanes::Color color = kAppIcons[index].color;
        if (pressed_icon == index) {
            color = Shade(color, kPressedShade);
        }
        if (open_app.has_value()) {
            color.alpha = 0;
        }
        world.GetComponent<svanes::SolidShape>(icons[index]).color = color;
        world.GetComponent<svanes::TextLabel>(icon_names[index]).visible =
            !open_app.has_value();
    }

    svanes::SolidShape &cover =
        world.GetComponent<svanes::SolidShape>(app_cover);
    svanes::SolidShape &bar = world.GetComponent<svanes::SolidShape>(home_bar);
    if (!open_app.has_value()) {
        cover.color.alpha = 0;
        bar.color.alpha = 0;
        app_shown = 0.0F;
        return;
    }

    const AppIcon &app = kAppIcons[*open_app];
    const float amount = std::clamp(app_amount, 0.0F, 1.0F);
    const svanes::Vector2D icon = IconCenter(*open_app);
    app_area = {.x = std::lerp(icon.x, 0.0F, amount),
                .y = std::lerp(icon.y, 0.0F, amount),
                .width = std::lerp(kIconSize, kScreenWidth, amount),
                .height = std::lerp(kIconSize, kScreenHeight, amount)};
    cover.color = app.color;
    cover.geometry = RoundedRectangle(
        app_area.width, app_area.height,
        std::lerp(kIconCornerRadius, kScreenCornerRadius, amount),
        {app_area.x, app_area.y});
    app_area.x += position.x;
    app_area.y += position.y;

    const float opened =
        app_motion == AppMotion::Opening
            ? std::clamp((open_seconds - kAppGrowSeconds) / kAppFadeSeconds,
                         0.0F, 1.0F)
            : 1.0F;
    app_shown = opened * std::clamp((amount - kAppContentShownAbove) /
                                        (1.0F - kAppContentShownAbove),
                                    0.0F, 1.0F);

    bar.color = kHomeBarColor;
    bar.color.alpha = static_cast<std::uint8_t>(
        static_cast<float>(kHomeBarColor.alpha) * opened);
}

void Phone::Place(svanes::Registry &world) {
    const float eased = SmoothStep(slide);
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
    for (std::size_t index = 0; index < icon_names.size(); ++index) {
        const svanes::Vector2D center = IconCenter(index);
        world.GetComponent<svanes::TextLabel>(icon_names[index]).position = {
            position.x + center.x,
            position.y + center.y + kIconSize * 0.5F + kIconNameGap};
    }
}

bool Phone::IsFullyHeld() const { return slide == 1.0F; }

float Phone::AppShown(App app) const {
    if (open_app != static_cast<std::size_t>(app)) {
        return 0.0F;
    }
    return app_shown;
}

svanes::Rectangle2D Phone::AppArea() const { return app_area; }

svanes::Rectangle2D Phone::ScreenRectangle() const {
    return {.x = position.x,
            .y = position.y,
            .width = kScreenWidth,
            .height = kScreenHeight};
}

} // namespace grindset

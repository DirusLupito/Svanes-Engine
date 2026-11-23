#include "reel_view.hpp"

#include "phone.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace grindset {

namespace {

constexpr std::int32_t kBandZ = Phone::kAppZ;
constexpr std::int32_t kShapeZ = Phone::kAppZ + 1;
constexpr std::int32_t kLabelZ = Phone::kAppZ + 2;

constexpr std::int32_t kWrapWidth = 330;
constexpr svanes::Color kWhite{.red = 255, .green = 255, .blue = 255};
constexpr svanes::Color kFaintWhite{
    .red = 255, .green = 255, .blue = 255, .alpha = 190};

constexpr svanes::Color kFactHeadingColor{.red = 250, .green = 210, .blue = 80};
constexpr svanes::Color kOrColor{.red = 30, .green = 30, .blue = 40};
constexpr float kOrRadius = 36.0F;

constexpr svanes::Color kGameSkyColor{.red = 120, .green = 190, .blue = 240};
constexpr svanes::Color kGameGroundColor{.red = 70, .green = 160, .blue = 70};
constexpr svanes::Color kGameRunnerColor{.red = 250, .green = 210, .blue = 60};
constexpr svanes::Color kGameObstacleColor{.red = 200, .green = 60, .blue = 60};
constexpr float kGroundHeight = 70.0F;
constexpr float kRunnerSize = 40.0F;
constexpr float kRunnerX = -110.0F;
constexpr float kRunnerJump = 70.0F;
constexpr float kRunnerJumpsPerSecond = 3.5F;
constexpr float kObstacleWidth = 28.0F;
constexpr float kObstacleHeight = 44.0F;
constexpr float kObstacleSpeed = 170.0F;
constexpr float kObstacleLoopExtra = 80.0F;
constexpr std::size_t kFirstObstacle = 2;

constexpr float kEmojiBounce = 16.0F;
constexpr float kEmojiBouncesPerSecond = 3.0F;

constexpr svanes::Color kSilhouetteColor{.red = 14, .green = 10, .blue = 18};
constexpr float kHeadRadius = 44.0F;
constexpr float kHeadY = 200.0F;
constexpr float kBodyWidth = 240.0F;
constexpr float kBodyHeight = 240.0F;
constexpr float kBodyCornerRadius = 70.0F;
constexpr float kBodyY = 390.0F;

/**
 * Fades an alpha by how much of the reel is showing.
 *
 * @param alpha The alpha when the reel is fully shown.
 * @param shown How much of the reel is showing, from 0 to 1.
 * @return The faded alpha.
 */
std::uint8_t Faded(std::uint8_t alpha, float shown) {
    return static_cast<std::uint8_t>(static_cast<float>(alpha) * shown);
}

/**
 * Mixes two colors.
 *
 * @param from The color at 0.
 * @param to The color at 1.
 * @param amount How far from the first color to the second, from 0 to 1.
 * @return The mixed color, fully opaque.
 */
svanes::Color Mix(svanes::Color from, svanes::Color to, float amount) {
    const auto mix = [amount](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(std::lround(
            std::lerp(static_cast<float>(a), static_cast<float>(b), amount)));
    };
    return {.red = mix(from.red, to.red),
            .green = mix(from.green, to.green),
            .blue = mix(from.blue, to.blue)};
}

/**
 * Hides a piece of a reel until it is set up again.
 *
 * @param color The piece's color.
 */
void Hide(svanes::Color &color) { color.alpha = 0; }

} // namespace

void ReelView::Initialize(svanes::Registry &world, const ReelFonts &reel_fonts,
                          svanes::Rectangle2D screen) {
    fonts = reel_fonts;
    width = screen.width;
    height = screen.height;
    const float band_height = height / static_cast<float>(kBandCount);
    for (std::size_t index = 0; index < bands.size(); ++index) {
        Piece &band = bands[index];
        band.entity = world.CreateEntity();
        band.offset = {0.0F,
                       -height * 0.5F +
                           band_height * (static_cast<float>(index) + 0.5F)};
        band.color = {.alpha = 0};
        world.AddComponent<svanes::Transform>(band.entity, svanes::Transform{});
        world.AddComponent<svanes::SolidShape>(
            band.entity, svanes::SolidShape{.color = {.alpha = 0},
                                            .geometry = svanes::Rectangle2D{
                                                .width = width,
                                                .height = band_height + 1.0F}});
        world.AddComponent<svanes::ZOrder>(band.entity, svanes::ZOrder{kBandZ});
        world.AddComponent<svanes::ClipRect>(band.entity, svanes::ClipRect{});
    }
    for (Piece &shape : shapes) {
        shape.entity = world.CreateEntity();
        shape.color = {.alpha = 0};
        world.AddComponent<svanes::Transform>(shape.entity,
                                              svanes::Transform{});
        world.AddComponent<svanes::SolidShape>(
            shape.entity,
            svanes::SolidShape{.color = {.alpha = 0},
                               .geometry = svanes::Rectangle2D{
                                   .width = 1.0F, .height = 1.0F}});
        world.AddComponent<svanes::ZOrder>(shape.entity,
                                           svanes::ZOrder{kShapeZ});
        world.AddComponent<svanes::ClipRect>(shape.entity, svanes::ClipRect{});
    }
    for (Piece &label : labels) {
        label.entity = world.CreateEntity();
        label.color = {.alpha = 0};
        world.AddComponent<svanes::TextLabel>(
            label.entity,
            svanes::TextLabel{.font = fonts.medium,
                              .alignment = svanes::TextAlignment::Center,
                              .visible = false});
        world.AddComponent<svanes::ZOrder>(label.entity,
                                           svanes::ZOrder{kLabelZ});
        world.AddComponent<svanes::ClipRect>(label.entity, svanes::ClipRect{});
    }
}

void ReelView::SetLabel(svanes::Registry &world, std::size_t index,
                        const std::string &text, svanes::FontHandle font,
                        svanes::Vector2D offset, svanes::Color color,
                        std::int32_t wrap_width) {
    Piece &piece = labels.at(index);
    piece.offset = offset;
    piece.color = color;
    svanes::TextLabel &label =
        world.GetComponent<svanes::TextLabel>(piece.entity);
    label.text = text;
    label.font = font;
    label.wrap_width = wrap_width;
}

void ReelView::SetShape(svanes::Registry &world, std::size_t index,
                        svanes::Geometry2D geometry, svanes::Vector2D offset,
                        svanes::Color color) {
    Piece &piece = shapes.at(index);
    piece.offset = offset;
    piece.color = color;
    world.GetComponent<svanes::SolidShape>(piece.entity).geometry =
        std::move(geometry);
}

void ReelView::Show(svanes::Registry &world, const Reel &reel) {
    format = reel.format;
    for (Piece &shape : shapes) {
        Hide(shape.color);
    }
    for (Piece &label : labels) {
        Hide(label.color);
    }

    for (std::size_t index = 0; index < bands.size(); ++index) {
        const float along =
            (static_cast<float>(index) + 0.5F) / static_cast<float>(kBandCount);
        const bool top_half = index < kBandCount / 2;
        svanes::Color color = Mix(reel.top_color, reel.bottom_color, along);
        if (format == ReelFormat::WouldYouRather) {
            color = top_half ? reel.top_color : reel.bottom_color;
        } else if (format == ReelFormat::Brainrot) {
            color = top_half
                        ? Mix(reel.top_color, reel.bottom_color, along * 2.0F)
                        : kGameSkyColor;
        }
        bands[index].color = color;
    }

    switch (format) {
    case ReelFormat::Pov:
        SetLabel(world, 0, reel.text, fonts.large, {0.0F, -40.0F}, kWhite,
                 kWrapWidth);
        SetLabel(world, 1, "#pov #fyp #grindset", fonts.small, {-50.0F, 372.0F},
                 kFaintWhite, 0);
        break;
    case ReelFormat::FakeFact:
        SetLabel(world, 0, "DID YOU KNOW?", fonts.large, {0.0F, -250.0F},
                 kFactHeadingColor, 0);
        SetLabel(world, 1, reel.text, fonts.medium, {0.0F, -30.0F}, kWhite,
                 kWrapWidth);
        SetLabel(world, 2, "follow for part 2", fonts.small, {0.0F, 230.0F},
                 kFaintWhite, 0);
        break;
    case ReelFormat::WouldYouRather:
        SetLabel(world, 0, "WOULD YOU RATHER", fonts.medium, {0.0F, -340.0F},
                 kWhite, 0);
        SetLabel(world, 1, reel.text, fonts.large, {0.0F, -190.0F}, kWhite,
                 kWrapWidth);
        SetLabel(world, 2, reel.second_text, fonts.large, {0.0F, 190.0F},
                 kWhite, kWrapWidth);
        SetShape(world, 0, svanes::Circle2D{.radius = kOrRadius}, {}, kWhite);
        SetLabel(world, 3, "OR", fonts.medium, {}, kOrColor, 0);
        break;
    case ReelFormat::Brainrot: {
        SetLabel(world, 0, reel.text, fonts.large, {0.0F, -height * 0.25F},
                 kWhite, kWrapWidth);
        const float ground_top = height * 0.5F - kGroundHeight;
        SetShape(world, 0,
                 svanes::Rectangle2D{.width = width, .height = kGroundHeight},
                 {0.0F, ground_top + kGroundHeight * 0.5F}, kGameGroundColor);
        SetShape(
            world, 1,
            svanes::Rectangle2D{.width = kRunnerSize, .height = kRunnerSize},
            {kRunnerX, ground_top - kRunnerSize * 0.5F}, kGameRunnerColor);
        const float loop = width + kObstacleLoopExtra;
        const std::size_t obstacles = kShapeCount - kFirstObstacle;
        for (std::size_t index = 0; index < obstacles; ++index) {
            SetShape(world, kFirstObstacle + index,
                     svanes::Rectangle2D{.width = kObstacleWidth,
                                         .height = kObstacleHeight},
                     {-loop * 0.5F + loop * static_cast<float>(index) /
                                         static_cast<float>(obstacles),
                      ground_top - kObstacleHeight * 0.5F},
                     kGameObstacleColor);
        }
        break;
    }
    case ReelFormat::EmojiStory:
        SetLabel(world, 0, reel.text, fonts.emoji, {0.0F, -70.0F}, kWhite, 0);
        SetLabel(world, 1, reel.second_text, fonts.medium, {0.0F, 90.0F},
                 kWhite, kWrapWidth);
        break;
    case ReelFormat::Guru:
        SetLabel(world, 0, reel.text, fonts.large, {0.0F, -150.0F}, kWhite,
                 kWrapWidth);
        SetLabel(world, 1, "@grindset.guru", fonts.small, {0.0F, -330.0F},
                 kFaintWhite, 0);
        SetShape(world, 0, svanes::Circle2D{.radius = kHeadRadius},
                 {0.0F, kHeadY}, kSilhouetteColor);
        SetShape(world, 1,
                 RoundedRectangle(kBodyWidth, kBodyHeight, kBodyCornerRadius),
                 {0.0F, kBodyY}, kSilhouetteColor);
        break;
    case ReelFormat::Count:
        throw std::logic_error("Cannot show a reel of no format.");
    }
}

svanes::Vector2D ReelView::Motion(bool label, std::size_t index,
                                  float seconds) const {
    if (format == ReelFormat::EmojiStory && label && index == 0) {
        return {0.0F, -std::abs(std::sin(seconds * kEmojiBouncesPerSecond *
                                         std::numbers::pi_v<float>)) *
                          kEmojiBounce};
    }
    if (format != ReelFormat::Brainrot || label) {
        return {};
    }
    if (index == 1) {
        return {0.0F, -std::abs(std::sin(seconds * kRunnerJumpsPerSecond *
                                         std::numbers::pi_v<float>)) *
                          kRunnerJump};
    }
    if (index < kFirstObstacle) {
        return {};
    }
    const float loop = width + kObstacleLoopExtra;
    const float start = shapes[index].offset.x;
    float moved =
        std::fmod(start + loop * 0.5F - seconds * kObstacleSpeed, loop);
    if (moved < 0.0F) {
        moved += loop;
    }
    return {moved - loop * 0.5F - start, 0.0F};
}

void ReelView::Place(svanes::Registry &world, svanes::Vector2D center,
                     svanes::Rectangle2D area, float shown,
                     float seconds) const {
    const auto place_shape = [&](const Piece &piece, svanes::Vector2D motion) {
        svanes::Transform &transform =
            world.GetComponent<svanes::Transform>(piece.entity);
        transform.x = center.x + piece.offset.x + motion.x;
        transform.y = center.y + piece.offset.y + motion.y;
        svanes::SolidShape &shape =
            world.GetComponent<svanes::SolidShape>(piece.entity);
        shape.color = piece.color;
        shape.color.alpha = Faded(piece.color.alpha, shown);
        world.GetComponent<svanes::ClipRect>(piece.entity).area = area;
    };
    for (const Piece &band : bands) {
        place_shape(band, {});
    }
    for (std::size_t index = 0; index < shapes.size(); ++index) {
        place_shape(shapes[index], Motion(false, index, seconds));
    }
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const Piece &piece = labels[index];
        const svanes::Vector2D motion = Motion(true, index, seconds);
        svanes::TextLabel &label =
            world.GetComponent<svanes::TextLabel>(piece.entity);
        label.position = {center.x + piece.offset.x + motion.x,
                          center.y + piece.offset.y + motion.y};
        label.color = piece.color;
        label.color.alpha = Faded(piece.color.alpha, shown);
        label.visible = label.color.alpha > 0;
        world.GetComponent<svanes::ClipRect>(piece.entity).area = area;
    }
}

} // namespace grindset

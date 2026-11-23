#include "reels_app.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/geometry/composite_geometry.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace grindset {

namespace {

constexpr std::int32_t kButtonZ = Phone::kAppZ + 3;
constexpr std::int32_t kPopupZ = Phone::kAppZ + 90;

constexpr float kButtonFromRight = 44.0F;
constexpr float kLikeFromBottom = 144.0F;
constexpr float kDislikeFromBottom = 64.0F;
constexpr float kButtonHitSize = 64.0F;
constexpr svanes::Color kButtonColor{.red = 255, .green = 255, .blue = 255};
constexpr svanes::Color kLikedColor{.red = 236, .green = 64, .blue = 90};
constexpr svanes::Color kDislikedColor{.red = 70, .green = 140, .blue = 240};

constexpr std::int64_t kPayCents = 50;
constexpr float kMinWatchSeconds = 3.0F;

constexpr float kPopupSeconds = 1.2F;
constexpr float kPopupFromTop = 100.0F;
constexpr float kPopupRise = 40.0F;
constexpr float kPopupWidth = 120.0F;
constexpr float kPopupHeight = 44.0F;
constexpr float kPopupCornerRadius = 18.0F;
constexpr svanes::Color kPopupPanelColor{
    .red = 0, .green = 0, .blue = 0, .alpha = 150};
constexpr svanes::Color kPopupTextColor{.red = 240, .green = 200, .blue = 80};

constexpr float kDragSampleSeconds = 0.06F;
constexpr float kFlickLookaheadSeconds = 0.3F;
constexpr float kSpringStiffness = 300.0F;
constexpr float kSpringDamping = 35.0F;
constexpr float kSpringRestDistance = 0.5F;
constexpr float kSpringRestSpeed = 5.0F;
constexpr float kRubberBandStiffness = 0.55F;


/**
 * Finds how far the reels move when pulled past the first reel, which is less
 * the further they are pulled, so they never reach a full screen.
 *
 * @param pulled How far the mouse has pulled the reels, in pixels.
 * @param screen_height The height of the phone's screen.
 * @return How far the reels move, in pixels.
 */
float RubberBand(float pulled, float screen_height) {
    return (1.0F -
            1.0F / (pulled * kRubberBandStiffness / screen_height + 1.0F)) *
           screen_height;
}

/**
 * Finds how far the mouse must have pulled the reels past the first reel for
 * them to have moved a given distance, undoing RubberBand.
 *
 * @param moved How far the reels have moved, in pixels. Must be less than
 * the screen's height.
 * @param screen_height The height of the phone's screen.
 * @return How far the mouse pulled them, in pixels.
 */
float RubberBandPull(float moved, float screen_height) {
    return screen_height / kRubberBandStiffness *
           (1.0F / (1.0F - moved / screen_height) - 1.0F);
}

/**
 * Builds a heart from two circles and a triangle, about 40 pixels across,
 * with the triangle's sides running along the circles' outer edges.
 *
 * @return The heart, centered on its middle.
 */
svanes::CompositeShape2D Heart() {
    svanes::CompositeShape2D heart;
    heart.parts.push_back(
        {.shape = svanes::Circle2D{.x = -8.0F, .y = -6.0F, .radius = 12.0F},
         .transform = {}});
    heart.parts.push_back(
        {.shape = svanes::Circle2D{.x = 8.0F, .y = -6.0F, .radius = 12.0F},
         .transform = {}});
    heart.parts.push_back(
        {.shape =
             svanes::Triangle2D{
                 .vertices = {{{-16.5F, 2.0F}, {16.5F, 2.0F}, {0.0F, 20.0F}}}},
         .transform = {}});
    return heart;
}

/**
 * Builds an arrow pointing down from a shaft and a triangular head, about 40
 * pixels tall.
 *
 * @return The arrow, centered on its middle.
 */
svanes::CompositeShape2D DownArrow() {
    svanes::CompositeShape2D arrow;
    arrow.parts.push_back(
        {.shape =
             svanes::Rectangle2D{.y = -8.0F, .width = 12.0F, .height = 22.0F},
         .transform = {}});
    arrow.parts.push_back(
        {.shape =
             svanes::Triangle2D{
                 .vertices = {{{-16.0F, 1.0F}, {16.0F, 1.0F}, {0.0F, 20.0F}}}},
         .transform = {}});
    return arrow;
}

/**
 * Finds where one of a reel's buttons is.
 *
 * @param reel_center Where the reel's center is.
 * @param screen Where the phone's screen is.
 * @param like True for the like button, false for the dislike button.
 * @return The button's center, in world coordinates.
 */
svanes::Vector2D ButtonCenter(svanes::Vector2D reel_center,
                              svanes::Rectangle2D screen, bool like) {
    return {reel_center.x + screen.width * 0.5F - kButtonFromRight,
            reel_center.y + screen.height * 0.5F -
                (like ? kLikeFromBottom : kDislikeFromBottom)};
}

/**
 * Creates one of a reel's buttons, hidden and clipped until it is placed.
 *
 * @param world The world to create it in.
 * @param shape The button's shape, centered on its middle.
 * @return The button's entity.
 */
svanes::Entity CreateButton(svanes::Registry &world,
                            svanes::CompositeShape2D shape) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(
        entity, svanes::SolidShape{.color = {.alpha = 0},
                                   .geometry = std::move(shape)});
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kButtonZ});
    world.AddComponent<svanes::ClipRect>(entity, svanes::ClipRect{});
    return entity;
}

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

/**
 * Moves, colors, fades, and clips one of a reel's buttons.
 *
 * @param world The world holding it.
 * @param entity The button's entity.
 * @param position Where the button's center goes, in world coordinates.
 * @param color The button's color when the app is fully shown.
 * @param area The area the app currently covers.
 * @param shown How much of the reel is showing, from 0 to 1.
 */
void PlaceButton(svanes::Registry &world, svanes::Entity entity,
                 svanes::Vector2D position, svanes::Color color,
                 svanes::Rectangle2D area, float shown) {
    svanes::Transform &transform =
        world.GetComponent<svanes::Transform>(entity);
    transform.x = position.x;
    transform.y = position.y;
    svanes::SolidShape &shape = world.GetComponent<svanes::SolidShape>(entity);
    shape.color = color;
    shape.color.alpha = Faded(color.alpha, shown);
    world.GetComponent<svanes::ClipRect>(entity).area = area;
}

} // namespace

void ReelsApp::Initialize(svanes::Registry &world, const ReelFonts &fonts,
                          const std::string &text_directory,
                          const Phone &phone) {
    generator.emplace(text_directory);
    const svanes::Rectangle2D screen = phone.ScreenRectangle();
    for (Slot &slot : slots) {
        slot.view.Initialize(world, fonts, screen);
        slot.like = CreateButton(world, Heart());
        slot.dislike = CreateButton(world, DownArrow());
    }

    popup_panel = world.CreateEntity();
    world.AddComponent<svanes::Transform>(popup_panel, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(
        popup_panel,
        svanes::SolidShape{.color = {.alpha = 0},
                           .geometry = RoundedRectangle(
                               kPopupWidth, kPopupHeight, kPopupCornerRadius)});
    world.AddComponent<svanes::ZOrder>(popup_panel, svanes::ZOrder{kPopupZ});
    world.AddComponent<svanes::ClipRect>(popup_panel, svanes::ClipRect{});

    popup_label = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        popup_label,
        svanes::TextLabel{.text = "+" + FormatDollars(kPayCents),
                          .font = fonts.medium,
                          .alignment = svanes::TextAlignment::Center,
                          .visible = false});
    world.AddComponent<svanes::ZOrder>(popup_label, svanes::ZOrder{kPopupZ});
    world.AddComponent<svanes::ClipRect>(popup_label, svanes::ClipRect{});
}

std::int64_t ReelsApp::Watch(float seconds, bool watching) {
    if (current != watched_reel) {
        watched_reel = current;
        watch_seconds = 0.0F;
    }
    if (!watching) {
        return 0;
    }
    watch_seconds += seconds;
    if (paid_reels.contains(current) || watch_seconds < kMinWatchSeconds) {
        return 0;
    }
    paid_reels.insert(current);
    popup_seconds = 0.0F;
    return kPayCents;
}

void ReelsApp::ShowPopup(svanes::Registry &world, svanes::Rectangle2D screen,
                         svanes::Rectangle2D area, float shown, float seconds) {
    float popup_shown = 0.0F;
    float rise = 0.0F;
    if (popup_seconds.has_value()) {
        *popup_seconds += seconds;
        const float progress = std::min(*popup_seconds / kPopupSeconds, 1.0F);
        rise = kPopupRise * (1.0F - (1.0F - progress) * (1.0F - progress));
        popup_shown = shown * std::min((1.0F - progress) * 2.0F, 1.0F);
        if (progress == 1.0F) {
            popup_seconds.reset();
        }
    }
    const svanes::Vector2D center{screen.x, screen.y - screen.height * 0.5F +
                                                kPopupFromTop - rise};

    svanes::Transform &transform =
        world.GetComponent<svanes::Transform>(popup_panel);
    transform.x = center.x;
    transform.y = center.y;
    svanes::SolidShape &panel =
        world.GetComponent<svanes::SolidShape>(popup_panel);
    panel.color = kPopupPanelColor;
    panel.color.alpha = Faded(kPopupPanelColor.alpha, popup_shown);
    world.GetComponent<svanes::ClipRect>(popup_panel).area = area;

    svanes::TextLabel &label =
        world.GetComponent<svanes::TextLabel>(popup_label);
    label.position = center;
    label.color = kPopupTextColor;
    label.color.alpha = Faded(kPopupTextColor.alpha, popup_shown);
    label.visible = popup_shown > 0.0F;
    world.GetComponent<svanes::ClipRect>(popup_label).area = area;
}

void ReelsApp::Release(float screen_height) {
    const DragSample &oldest = drag_samples.front();
    const DragSample &newest = drag_samples.back();
    const float sample_seconds = newest.seconds - oldest.seconds;
    speed = sample_seconds > 0.0F
                ? (newest.offset - oldest.offset) / sample_seconds
                : 0.0F;
    const float reach = offset + speed * kFlickLookaheadSeconds;
    if (reach > screen_height * 0.5F && current > 0) {
        --current;
        offset -= screen_height;
    } else if (reach < -screen_height * 0.5F) {
        ++current;
        offset += screen_height;
    }
    drag_anchor_y.reset();
}

void ReelsApp::Settle(float seconds) {
    if (drag_anchor_y.has_value()) {
        return;
    }
    const float pull = -kSpringStiffness * offset - kSpringDamping * speed;
    speed += pull * seconds;
    offset += speed * seconds;
    if (std::abs(offset) < kSpringRestDistance &&
        std::abs(speed) < kSpringRestSpeed) {
        offset = 0.0F;
        speed = 0.0F;
    }
}

void ReelsApp::TrackInput(const svanes::InputManager &input,
                          svanes::Rectangle2D screen) {
    const svanes::Vector2D mouse = input.MousePosition();
    if (drag_anchor_y.has_value()) {
        const float pulled = mouse.y - *drag_anchor_y;
        if (current == 0 && pulled > 0.0F) {
            offset = RubberBand(pulled, screen.height);
        } else {
            offset = std::clamp(pulled, -screen.height, screen.height);
        }
        drag_samples.push_back({.seconds = elapsed, .offset = offset});
        std::erase_if(drag_samples, [this](const DragSample &sample) {
            return sample.seconds < elapsed - kDragSampleSeconds;
        });
        if (!input.IsMouseButtonDown(svanes::MouseButton::Left)) {
            Release(screen.height);
        }
        return;
    }

    if (TrackButtons(input, screen)) {
        return;
    }

    if (input.WasPressed(svanes::Key::Up) && current > 0) {
        --current;
        offset = std::max(offset - screen.height, -screen.height);
        return;
    }
    if (input.WasPressed(svanes::Key::Down)) {
        ++current;
        offset = std::min(offset + screen.height, screen.height);
        return;
    }

    if (!input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        return;
    }
    const float top = screen.y - screen.height * 0.5F;
    const float swipe_strip = top + screen.height - Phone::kHomeSwipeHeight;
    if (std::abs(mouse.x - screen.x) <= screen.width * 0.5F && mouse.y >= top &&
        mouse.y < swipe_strip) {
        const bool past_first = current == 0 && offset > 0.0F;
        drag_anchor_y =
            mouse.y -
            (past_first ? RubberBandPull(offset, screen.height) : offset);
        speed = 0.0F;
        drag_samples = {{.seconds = elapsed, .offset = offset}};
    }
}

bool ReelsApp::TrackButtons(const svanes::InputManager &input,
                            svanes::Rectangle2D screen) {
    const svanes::Vector2D mouse = input.MousePosition();
    if (input.WasMouseButtonReleased(svanes::MouseButton::Left) &&
        pressed_button.has_value()) {
        const Reaction pressed = *pressed_button;
        pressed_button.reset();
        if (ButtonUnderMouse(mouse, screen) != pressed) {
            return true;
        }
        const auto found = reactions.find(current);
        if (found != reactions.end() && found->second == pressed) {
            reactions.erase(found);
        } else {
            reactions[current] = pressed;
        }
        return true;
    }
    if (input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        pressed_button = ButtonUnderMouse(mouse, screen);
        return pressed_button.has_value();
    }
    return false;
}

std::optional<ReelsApp::Reaction>
ReelsApp::ButtonUnderMouse(svanes::Vector2D mouse,
                           svanes::Rectangle2D screen) const {
    const svanes::Vector2D reel_center{screen.x, screen.y + offset};
    for (const Reaction reaction : {Reaction::Like, Reaction::Dislike}) {
        const svanes::Vector2D center =
            ButtonCenter(reel_center, screen, reaction == Reaction::Like);
        if (std::abs(mouse.x - center.x) <= kButtonHitSize * 0.5F &&
            std::abs(mouse.y - center.y) <= kButtonHitSize * 0.5F) {
            return reaction;
        }
    }
    return std::nullopt;
}

bool ReelsApp::IsOverButton(svanes::Vector2D mouse, const Phone &phone) const {
    return phone.AppShown(Phone::App::Instagram) == 1.0F &&
           ButtonUnderMouse(mouse, phone.ScreenRectangle()).has_value();
}

std::int64_t ReelsApp::Update(svanes::Registry &world,
                              const svanes::InputManager &input,
                              const Phone &phone, float seconds) {
    const svanes::Rectangle2D screen = phone.ScreenRectangle();
    const svanes::Rectangle2D area = phone.AppArea();
    const float shown = phone.AppShown(Phone::App::Instagram);

    elapsed += seconds;
    const bool watching = shown == 1.0F && phone.IsFullyHeld();
    if (watching) {
        TrackInput(input, screen);
    } else {
        pressed_button.reset();
        if (drag_anchor_y.has_value()) {
            Release(screen.height);
        }
    }
    Settle(seconds);
    const std::int64_t earned_cents = Watch(seconds, watching);

    for (std::size_t index = 0; index < slots.size(); ++index) {
        Slot &slot = slots[index];
        const float place = static_cast<float>(index) - 1.0F;
        const bool exists = index > 0 || current > 0;
        const std::uint32_t reel =
            exists ? current + static_cast<std::uint32_t>(index) - 1 : 0;
        const float slot_shown = exists ? shown : 0.0F;
        const svanes::Vector2D center{screen.x, screen.y + offset +
                                                    place * screen.height};

        if (exists && slot.showing != reel) {
            slot.view.Show(world, generator->Generate(reel));
            slot.showing = reel;
        }
        slot.view.Place(world, center, area, slot_shown, elapsed);

        const auto reaction = reactions.find(reel);
        const bool liked =
            reaction != reactions.end() && reaction->second == Reaction::Like;
        const bool disliked = reaction != reactions.end() &&
                              reaction->second == Reaction::Dislike;
        PlaceButton(world, slot.like, ButtonCenter(center, screen, true),
                    liked ? kLikedColor : kButtonColor, area, slot_shown);
        PlaceButton(world, slot.dislike, ButtonCenter(center, screen, false),
                    disliked ? kDislikedColor : kButtonColor, area, slot_shown);
    }

    ShowPopup(world, screen, area, shown, seconds);
    return earned_cents;
}

} // namespace grindset

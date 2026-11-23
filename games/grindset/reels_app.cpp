#include "reels_app.hpp"

#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/render_system.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace grindset {

namespace {

constexpr std::int32_t kCardZ = Phone::kAppZ;
constexpr std::int32_t kLabelZ = Phone::kAppZ + 1;
constexpr std::int32_t kPopupZ = Phone::kAppZ + 90;

constexpr std::int64_t kPayCents = 50;
constexpr float kMinWatchSeconds = 3.0F;

constexpr float kPopupSeconds = 1.2F;
constexpr float kPopupFromTop = 100.0F;
constexpr float kPopupRise = 40.0F;
constexpr float kPopupWidth = 120.0F;
constexpr float kPopupHeight = 44.0F;
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

constexpr svanes::Color kLabelColor{.red = 255, .green = 255, .blue = 255};

constexpr std::array<svanes::Color, 5> kReelColors{{
    {.red = 196, .green = 70, .blue = 92},
    {.red = 52, .green = 110, .blue = 180},
    {.red = 46, .green = 140, .blue = 96},
    {.red = 170, .green = 118, .blue = 40},
    {.red = 112, .green = 72, .blue = 170},
}};

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

void ReelsApp::Initialize(svanes::Registry &world, svanes::FontHandle font,
                          const Phone &phone) {
    const svanes::Rectangle2D screen = phone.ScreenRectangle();
    for (Slot &slot : slots) {
        slot.card = world.CreateEntity();
        world.AddComponent<svanes::Transform>(slot.card, svanes::Transform{});
        world.AddComponent<svanes::SolidShape>(
            slot.card, svanes::SolidShape{.color = {.alpha = 0},
                                          .geometry = svanes::Rectangle2D{
                                              .width = screen.width,
                                              .height = screen.height}});
        world.AddComponent<svanes::ZOrder>(slot.card, svanes::ZOrder{kCardZ});
        world.AddComponent<svanes::ClipRect>(slot.card, svanes::ClipRect{});

        slot.label = world.CreateEntity();
        world.AddComponent<svanes::TextLabel>(
            slot.label,
            svanes::TextLabel{.font = font,
                              .alignment = svanes::TextAlignment::Center,
                              .visible = false});
        world.AddComponent<svanes::ZOrder>(slot.label, svanes::ZOrder{kLabelZ});
        world.AddComponent<svanes::ClipRect>(slot.label, svanes::ClipRect{});
    }

    popup_panel = world.CreateEntity();
    world.AddComponent<svanes::Transform>(popup_panel, svanes::Transform{});
    world.AddComponent<svanes::SolidShape>(
        popup_panel, svanes::SolidShape{
                         .color = {.alpha = 0},
                         .geometry = RoundedRectangle(kPopupWidth, kPopupHeight,
                                                      kPopupHeight * 0.5F)});
    world.AddComponent<svanes::ZOrder>(popup_panel, svanes::ZOrder{kPopupZ});
    world.AddComponent<svanes::ClipRect>(popup_panel, svanes::ClipRect{});

    popup_label = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(
        popup_label,
        svanes::TextLabel{.text = "+" + FormatDollars(kPayCents),
                          .font = font,
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
    if (current < first_unpaid_reel || watch_seconds < kMinWatchSeconds) {
        return 0;
    }
    first_unpaid_reel = current + 1;
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
    } else if (drag_anchor_y.has_value()) {
        Release(screen.height);
    }
    Settle(seconds);
    const std::int64_t earned_cents = Watch(seconds, watching);

    for (std::size_t index = 0; index < slots.size(); ++index) {
        const Slot &slot = slots[index];
        const float place = static_cast<float>(index) - 1.0F;
        const bool exists = index > 0 || current > 0;
        const std::uint32_t reel =
            exists ? current + static_cast<std::uint32_t>(index) - 1 : 0;
        const float slot_shown = exists ? shown : 0.0F;
        const svanes::Vector2D center{screen.x, screen.y + offset +
                                                    place * screen.height};

        svanes::Transform &transform =
            world.GetComponent<svanes::Transform>(slot.card);
        transform.x = center.x;
        transform.y = center.y;
        svanes::SolidShape &card =
            world.GetComponent<svanes::SolidShape>(slot.card);
        card.color = kReelColors[reel % kReelColors.size()];
        card.color.alpha = Faded(card.color.alpha, slot_shown);
        world.GetComponent<svanes::ClipRect>(slot.card).area = area;

        svanes::TextLabel &label =
            world.GetComponent<svanes::TextLabel>(slot.label);
        label.text = "Reel " + std::to_string(reel + 1);
        label.position = center;
        label.color = kLabelColor;
        label.color.alpha = Faded(kLabelColor.alpha, slot_shown);
        label.visible = slot_shown > 0.0F;
        world.GetComponent<svanes::ClipRect>(slot.label).area = area;
    }

    ShowPopup(world, screen, area, shown, seconds);
    return earned_cents;
}

} // namespace grindset

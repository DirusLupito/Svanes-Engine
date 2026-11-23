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

constexpr svanes::Color kLabelColor{.red = 255, .green = 255, .blue = 255};

constexpr std::array<svanes::Color, 5> kReelColors{{
    {.red = 196, .green = 70, .blue = 92},
    {.red = 52, .green = 110, .blue = 180},
    {.red = 46, .green = 140, .blue = 96},
    {.red = 170, .green = 118, .blue = 40},
    {.red = 112, .green = 72, .blue = 170},
}};

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
                                          .geometry = RoundedRectangle(
                                              screen.width, screen.height,
                                              Phone::kScreenCornerRadius)});
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
}

void ReelsApp::Snap(float screen_height) {
    if (offset > screen_height * 0.5F) {
        --current;
    } else if (offset < -screen_height * 0.5F) {
        ++current;
    }
    offset = 0.0F;
    drag_start_y.reset();
}

void ReelsApp::TrackInput(const svanes::InputManager &input,
                          svanes::Rectangle2D screen) {
    const svanes::Vector2D mouse = input.MousePosition();
    if (drag_start_y.has_value()) {
        const float lowest = current > 0 ? screen.height : 0.0F;
        offset = std::clamp(mouse.y - *drag_start_y, -screen.height, lowest);
        if (!input.IsMouseButtonDown(svanes::MouseButton::Left)) {
            Snap(screen.height);
        }
        return;
    }

    if (input.WasPressed(svanes::Key::Up) && current > 0) {
        --current;
        return;
    }
    if (input.WasPressed(svanes::Key::Down)) {
        ++current;
        return;
    }

    if (!input.WasMouseButtonPressed(svanes::MouseButton::Left)) {
        return;
    }
    const float top = screen.y - screen.height * 0.5F;
    const float swipe_strip = top + screen.height - Phone::kHomeSwipeHeight;
    if (std::abs(mouse.x - screen.x) <= screen.width * 0.5F && mouse.y >= top &&
        mouse.y < swipe_strip) {
        drag_start_y = mouse.y;
    }
}

void ReelsApp::Update(svanes::Registry &world,
                      const svanes::InputManager &input, const Phone &phone) {
    const svanes::Rectangle2D screen = phone.ScreenRectangle();
    const svanes::Rectangle2D area = phone.AppArea();
    const float shown = phone.AppShown(Phone::App::Instagram);

    if (shown == 1.0F && phone.IsFullyHeld()) {
        TrackInput(input, screen);
    } else if (drag_start_y.has_value()) {
        Snap(screen.height);
    }

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
}

} // namespace grindset

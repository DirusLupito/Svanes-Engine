#include "pause_menu.hpp"

#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/MenuUtilities/text_label.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <stdexcept>
#include <string>
#include <variant>

namespace {

constexpr std::array<const char*, 3> kSpeedNames{"0.5x", "1x", "2x"};

constexpr std::int32_t kBackdropZ = 1000;
constexpr std::int32_t kHighlightZ = 1001;
constexpr std::int32_t kButtonZ = 1002;
constexpr std::int32_t kLabelZ = 1003;

constexpr float kLargeTextPoints = 28.0F;
constexpr float kSmallTextPoints = 18.0F;
constexpr float kHighlightBorder = 6.0F;
constexpr float kCornerMargin = 16.0F;

constexpr svanes::Color kBackdropColor{.red = 20, .green = 20, .blue = 30, .alpha = 220};
constexpr svanes::Color kButtonColor{.red = 60, .green = 60, .blue = 80};
constexpr svanes::Color kAppliedColor{.red = 70, .green = 140, .blue = 90};
constexpr svanes::Color kDisabledColor{.red = 45, .green = 45, .blue = 50};
constexpr svanes::Color kHighlightColor{.red = 240, .green = 220, .blue = 80};
constexpr svanes::Color kQuitColor{.red = 140, .green = 50, .blue = 50};
constexpr svanes::Color kTextColor{.red = 255, .green = 255, .blue = 255};
constexpr svanes::Color kDisabledTextColor{.red = 120, .green = 120, .blue = 120};

/**
 * Loads a menu font, failing loudly rather than drawing nothing.
 * @param fonts The font manager to load through.
 * @param points The font size.
 * @return The loaded font.
 * @throws std::runtime_error if the font cannot be loaded.
 */
svanes::FontHandle LoadMenuFont(svanes::FontManager& fonts, float points)
{
    const std::string path = svanes::AssetPath(GOOSE_ASSETS_DIR) + "/fonts/consola.ttf";
    const svanes::FontHandle font = fonts.LoadFont(path, points);
    if (font.id == 0) {
        throw std::runtime_error("PauseMenu could not load its font from " + path);
    }
    return font;
}

/**
 * Creates a hidden rectangle drawn above the world.
 * @param world The registry to create it in.
 * @param z_order Its draw order.
 * @return The new entity.
 */
svanes::Entity CreateShape(svanes::Registry& world, std::int32_t z_order)
{
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Transform>(entity);
    world.AddComponent<svanes::SolidShape>(entity, svanes::SolidShape{
        .color = {.alpha = 0},
        .geometry = svanes::Rectangle2D{},
    });
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    return entity;
}

/**
 * Creates a hidden label drawn above the menu's rectangles.
 * @param world The registry to create it in.
 * @param font The font to draw with.
 * @param alignment Which point of the text its position names.
 * @return The new entity.
 */
svanes::Entity CreateLabel(svanes::Registry& world, svanes::FontHandle font, svanes::TextAlignment alignment)
{
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::TextLabel>(entity, svanes::TextLabel{
        .font = font,
        .alignment = alignment,
        .visible = false,
    });
    world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{kLabelZ});
    return entity;
}

/**
 * @param viewport The viewport in screen coordinates.
 * @return The viewport's top-left corner in screen coordinates.
 */
svanes::Vector2D TopLeft(const svanes::Rectangle2D& viewport)
{
    return {viewport.x - viewport.width * 0.5F, viewport.y - viewport.height * 0.5F};
}

/**
 * @param area A rectangle given by its center and size.
 * @param point A point in the same coordinates.
 * @return Whether the point lies inside the rectangle.
 */
bool Contains(const svanes::Rectangle2D& area, svanes::Vector2D point)
{
    return point.x >= area.x - area.width * 0.5F && point.x <= area.x + area.width * 0.5F &&
        point.y >= area.y - area.height * 0.5F && point.y <= area.y + area.height * 0.5F;
}

/**
 * @param color A color.
 * @param visible Whether it should be seen.
 * @return The color, made fully transparent when hidden.
 */
svanes::Color Shown(svanes::Color color, bool visible)
{
    if (!visible) {
        color.alpha = 0;
    }
    return color;
}

/**
 * Covers a viewport-relative area with a rectangle entity.
 * @param frame The frame's world and camera.
 * @param entity The rectangle entity.
 * @param area The area, in pixels from the viewport's top-left corner.
 * @param color The color to fill it with.
 */
void PlaceShape(const svanes::FrameContext& frame, svanes::Entity entity, const svanes::Rectangle2D& area, svanes::Color color)
{
    const svanes::Vector2D origin = TopLeft(frame.camera.Viewport());
    const svanes::Rectangle2D world_area = frame.camera.ScreenToWorld(
        {origin.x + area.x, origin.y + area.y, area.width, area.height}
    );
    svanes::Transform& transform = frame.world.GetComponent<svanes::Transform>(entity);
    transform.x = world_area.x;
    transform.y = world_area.y;
    svanes::SolidShape& shape = frame.world.GetComponent<svanes::SolidShape>(entity);
    shape.color = color;
    shape.geometry = svanes::Rectangle2D{.width = world_area.width, .height = world_area.height};
}

/**
 * Updates a label's text, placement, and color.
 * @param world The registry holding the label.
 * @param entity The label entity.
 * @param text The text to show.
 * @param position Where to draw it, in pixels from the viewport's top-left corner.
 * @param color The text color.
 * @param visible Whether to draw it.
 */
void PlaceLabel(
    svanes::Registry& world, svanes::Entity entity, const std::string& text,
    svanes::Vector2D position, svanes::Color color, bool visible
)
{
    svanes::TextLabel& label = world.GetComponent<svanes::TextLabel>(entity);
    label.text = text;
    label.position = position;
    label.color = color;
    label.visible = visible;
}

}

PauseMenu::PauseMenu(svanes::GameContext& context)
{
    const svanes::FontHandle large = LoadMenuFont(context.fonts, kLargeTextPoints);
    const svanes::FontHandle small = LoadMenuFont(context.fonts, kSmallTextPoints);
    svanes::Registry& world = context.world;

    backdrop = CreateShape(world, kBackdropZ);
    highlight = CreateShape(world, kHighlightZ);
    for (Button& button : speed_buttons) {
        button.shape = CreateShape(world, kButtonZ);
        button.label = CreateLabel(world, large, svanes::TextAlignment::Center);
    }
    quit_button.shape = CreateShape(world, kButtonZ);
    quit_button.label = CreateLabel(world, large, svanes::TextAlignment::Center);
    title = CreateLabel(world, large, svanes::TextAlignment::Center);
    caption = CreateLabel(world, small, svanes::TextAlignment::Center);
    note = CreateLabel(world, small, svanes::TextAlignment::Center);
    hint = CreateLabel(world, small, svanes::TextAlignment::Center);
    speed_label = CreateLabel(world, large, svanes::TextAlignment::TopRight);
}

void PauseMenu::Layout(const svanes::Rectangle2D& viewport)
{
    const float center_x = viewport.width * 0.5F;
    const float center_y = viewport.height * 0.5F;
    backdrop_area = {center_x, center_y, 480.0F, 400.0F};
    for (std::size_t index = 0; index < speed_buttons.size(); ++index) {
        const float offset = (static_cast<float>(index) - 1.0F) * 130.0F;
        speed_buttons[index].area = {center_x + offset, center_y - 30.0F, 110.0F, 56.0F};
    }
    quit_button.area = {center_x, center_y + 100.0F, 220.0F, 56.0F};
}

void PauseMenu::HandleInput(const svanes::FrameContext& frame, bool alone)
{
    if (frame.input.WasPressed(svanes::Key::Escape)) {
        open = !open;
        highlighted = speed;
        return;
    }
    if (!open) {
        return;
    }

    const svanes::Rectangle2D viewport = frame.camera.Viewport();
    Layout(viewport);
    const svanes::Vector2D origin = TopLeft(viewport);
    const svanes::Vector2D mouse = frame.input.MousePosition();
    const svanes::Vector2D pointer{mouse.x - origin.x, mouse.y - origin.y};
    const bool clicked = frame.input.WasMouseButtonPressed(svanes::MouseButton::Left);

    if (frame.input.WasPressed(svanes::Key::Q) || (clicked && Contains(quit_button.area, pointer))) {
        quit_requested = true;
    }
    if (!alone) {
        highlighted = speed;
        return;
    }

    auto index = static_cast<std::size_t>(highlighted);
    if ((frame.input.WasPressed(svanes::Key::Left) || frame.input.WasPressed(svanes::Key::A)) && index > 0) {
        --index;
    }
    if ((frame.input.WasPressed(svanes::Key::Right) || frame.input.WasPressed(svanes::Key::D)) &&
        index + 1 < speed_buttons.size()) {
        ++index;
    }
    bool clicked_option = false;
    if (clicked) {
        for (std::size_t option = 0; option < speed_buttons.size(); ++option) {
            if (Contains(speed_buttons[option].area, pointer)) {
                index = option;
                clicked_option = true;
            }
        }
    }
    highlighted = static_cast<GooseSpeed>(index);
    if (clicked_option || frame.input.WasPressed(svanes::Key::Enter) ||
        frame.input.WasPressed(svanes::Key::KeypadEnter)) {
        speed = highlighted;
    }
}

void PauseMenu::Draw(const svanes::FrameContext& frame, bool alone)
{
    const svanes::Rectangle2D viewport = frame.camera.Viewport();
    Layout(viewport);
    const float center_x = viewport.width * 0.5F;
    const float center_y = viewport.height * 0.5F;

    PlaceShape(frame, backdrop, backdrop_area, Shown(kBackdropColor, open));
    PlaceLabel(frame.world, title, alone ? "PAUSED" : "MENU", {center_x, center_y - 150.0F}, kTextColor, open);
    PlaceLabel(frame.world, caption, "Game speed", {center_x, center_y - 90.0F},
        alone ? kTextColor : kDisabledTextColor, open);

    for (std::size_t index = 0; index < speed_buttons.size(); ++index) {
        const Button& button = speed_buttons[index];
        svanes::Color fill = kButtonColor;
        if (!alone) {
            fill = kDisabledColor;
        } else if (index == static_cast<std::size_t>(speed)) {
            fill = kAppliedColor;
        }
        PlaceShape(frame, button.shape, button.area, Shown(fill, open));
        PlaceLabel(frame.world, button.label, kSpeedNames[index], {button.area.x, button.area.y},
            alone ? kTextColor : kDisabledTextColor, open);
    }

    const svanes::Rectangle2D& chosen = speed_buttons[static_cast<std::size_t>(highlighted)].area;
    PlaceShape(frame, highlight,
        {chosen.x, chosen.y, chosen.width + 2.0F * kHighlightBorder, chosen.height + 2.0F * kHighlightBorder},
        Shown(kHighlightColor, open && alone));

    PlaceLabel(frame.world, note,
        alone ? "Left/Right to choose, Enter to apply" : "Only available while playing alone",
        {center_x, center_y + 25.0F}, alone ? kTextColor : kDisabledTextColor, open);

    PlaceShape(frame, quit_button.shape, quit_button.area, Shown(kQuitColor, open));
    PlaceLabel(frame.world, quit_button.label, "Quit (Q)", {quit_button.area.x, quit_button.area.y}, kTextColor, open);
    PlaceLabel(frame.world, hint, "Esc to resume", {center_x, center_y + 165.0F}, kTextColor, open);

    PlaceLabel(frame.world, speed_label, kSpeedNames[static_cast<std::size_t>(speed)],
        {viewport.width - kCornerMargin, kCornerMargin}, kTextColor, speed != GooseSpeed::Normal);
}

bool PauseMenu::IsOpen() const
{
    return open;
}

bool PauseMenu::TakeQuitRequest()
{
    const bool requested = quit_requested;
    quit_requested = false;
    return requested;
}

GooseSpeed PauseMenu::Speed() const
{
    return speed;
}

void PauseMenu::ResetSpeed()
{
    speed = GooseSpeed::Normal;
    highlighted = GooseSpeed::Normal;
}

#pragma once

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <string>

namespace svanes {

/**
 * Enumeration for positioning the text around the specified position.
 *
 * MEMBERS:
 * - TopLeft: Places the top-left corner at the specified position.
 * - TopCenter: Places the center of the top edge at the specified position.
 * - TopRight: Places the top-right corner at the specified position.
 * - CenterLeft: Places the center of the left edge at the specified position.
 * - Center: Places the center of the text at the specified position.
 * - CenterRight: Places the center of the right edge at the specified position.
 * - BottomLeft: Places the bottom-left corner at the specified position.
 * - BottomCenter: Places the center of the bottom edge at the specified
 * position.
 * - BottomRight: Places the bottom-right corner at the specified position.
 */
enum class TextAlignment : std::uint8_t {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
};

/**
 * Represents a text label that can be rendered on the screen.
 *
 * FIELDS:
 * - text: The string content of the label.
 * - position: Position at which to place the corner, edge midpoint, or center
 *   selected by alignment. For example, TopCenter places the midpoint
 *   of the text rectangle's top edge here.
 * - color: The color of the text, including alpha for transparency.
 * - font: A handle to the font used for rendering the text.
 * - alignment: The horizontal and vertical alignment relative to position.
 * - visible: A flag indicating whether the label should be rendered.
 */
struct TextLabel {
    std::string text;
    Vector2D position{};
    Color color{};
    FontHandle font{};
    TextAlignment alignment = TextAlignment::TopLeft;
    bool visible = true;
};

} // namespace svanes

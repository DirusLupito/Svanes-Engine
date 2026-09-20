#pragma once

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <string>

namespace svanes {

/**
 * Enumeration for horizontal text alignment options.
 *
 * MEMBERS:
 * - Left: Aligns the text to the left edge of the specified position.
 * - Center: Centers the text horizontally around the specified position.
 * - Right: Aligns the text to the right edge of the specified position.
 */
enum class TextAlignment : std::uint8_t {
    Left,
    Center,
    Right,
};

/**
 * Represents a text label that can be rendered on the screen.
 *
 * FIELDS:
 * - text: The string content of the label.
 * - position: The position in pixels relative to the viewport's top-left
 * corner. This is the text's top-left for left alignment, top-center for
 * center alignment, and top-right for right alignment.
 * - color: The color of the text, including alpha for transparency.
 * - font: A handle to the font used for rendering the text.
 * - xAlignment: The horizontal alignment of the text relative to its position.
 * - visible: A flag indicating whether the label should be rendered.
 */
struct TextLabel {
    std::string text;
    Vector2D position{};
    Color color{};
    FontHandle font{};
    TextAlignment xAlignment = TextAlignment::Left;
    bool visible = true;
};

} // namespace svanes

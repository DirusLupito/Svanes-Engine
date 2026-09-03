/**
 * Basic data types used for rendering.
 * Includes structs for colors (Color), rectangles (Rectangle),
 * texture handles (TextureHandle), and image data (ImageData).
 * @file basic_render_types.hpp
 */

#pragma once

#include <cstdint>
#include <vector>

namespace svanes {

/**
 * Represents a color with red, green, blue, and alpha components.
 * Each component is an 8-bit unsigned integer (0-255).
 * The default color is black with full opacity (alpha = 255).
 */
struct Color {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;
};

/**
 * Represents a rectangle defined by its top-left corner (x, y) and its dimensions (width, height).
 * All values are floating-point numbers. Units may not necessarily be pixels.
 */
struct Rectangle {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

/**
 * Represents a handle to a texture resource
 * The 'id' is a unique identifier for the texture, assigned by the rendering system.
 * The texture manager shall return a valid TextureHandle when a texture is loaded.
 * Internally, this should be uniquely associated with an SDL_Texture* or similar resource.
 */
struct TextureHandle {
    std::uint32_t id = 0;
};

/**
 * Represents image data, including its dimensions and pixel data in RGBA format.
 * The pixel data is stored as a vector of 8-bit unsigned integers.
 * May be 
 */
struct ImageData {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::uint8_t> rgba_pixels;
};

}

/**
 * Basic data types used for rendering.
 * Includes structs for colors (Color),
 * texture handles (TextureHandle), and image data (ImageData).
 * @file basic_render_types.hpp
 */

#pragma once

#include <cstdint>
#include <vector>

namespace svanes {

/**
 * Represents the rule used to combine a rendered color with the color already
 * present at the same location on the screen.
 *
 * Alpha blending uses the alpha component of the rendered color as its opacity.
 * A color with alpha = 0 is completely transparent, so the color already on
 * the screen remains visible. A color with alpha = 255 is completely opaque,
 * so it covers the color already on the screen. Values between these extremes
 * produce a mixture of the rendered color and the existing color. This is the
 * usual mode for sprites, shapes, and other objects that should appear in
 * front of the scene without completely hiding it.
 *
 * Additive blending adds the rendered color to the color already on the
 * screen. The alpha component controls how much color is added, but the
 * existing color is not made darker or hidden. This is useful for effects that
 * represent light or energy, such as glows, fire, sparks, laser beams, etc.
 */
enum class BlendMode : std::uint8_t {
    Alpha,
    Additive,
};

/**
 * Represents a color with red, green, blue, and alpha components.
 * Each component is an 8-bit unsigned integer (0-255).
 * The default color is black with full opacity (alpha = 255).
 *
 * FIELDS:
 * - red: The red component of the color.
 * - green: The green component of the color.
 * - blue: The blue component of the color.
 * - alpha: The opacity component of the color.
 */
struct Color {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;
};

/**
 * Represents a handle to a texture resource
 * The 'id' is a unique identifier for the texture, assigned by the rendering system.
 * The texture manager shall return a valid TextureHandle when a texture is loaded.
 * Internally, this should be uniquely associated with an SDL_Texture* or similar resource.
 *
 * FIELDS:
 * - id: The unique identifier assigned to the texture resource.
 */
struct TextureHandle {
    std::uint32_t id = 0;
};

/**
 * Represents image data, including its dimensions and pixel data in RGBA format.
 * The pixel data is stored as a vector of 8-bit unsigned integers.
 * May be used to create textures or manipulate images before rendering.
 *
 * FIELDS:
 * - width: The width of the image in pixels.
 * - height: The height of the image in pixels.
 * - rgba_pixels: A vector containing the pixel data in RGBA format.
 */
struct ImageData {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::uint8_t> rgba_pixels;
};

}

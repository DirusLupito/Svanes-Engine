/**
 * Public header file reporting the contract for calling
 * the render queue from external game code.
 * @file render_queue.hpp
 */

#pragma once

#include <svanes/render/basic_render_types.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace svanes::internal {

class RenderQueueExecutor;

}

namespace svanes {

/**
 * Represents a queue of rendering commands to be executed by the rendering system.
 */
class RenderQueue final {
public:
    /**
     * Clears the render queue and adds a command to clear the screen with the specified color.
     * @param color The color to clear the screen with.
     */
    void Clear(Color color);

    /**
     * Adds a command to draw a rectangle to the render queue.
     * @param destination The destination rectangle.
     * @param color The color of the rectangle.
     */
    void DrawRectangle(Rectangle destination, Color color);

    /**
     * Adds a command to draw a texture to the render queue.
     * This will draw the entire texture to the specified destination rectangle.
     * @param texture The handle of the texture to draw.
     * @param destination The destination rectangle where the texture will be drawn.
     */
    void DrawTexture(TextureHandle texture, Rectangle destination);

    /**
     * Adds a command to draw a texture to the render queue with a specified source rectangle.
     * This will draw only the source region of the texture to the specified destination rectangle.
     * @param texture The handle of the texture to draw.
     * @param source The source rectangle from the texture to draw.
     * @param destination The destination rectangle where the texture will be drawn.
     */
    void DrawTexture(TextureHandle texture, Rectangle source, Rectangle destination);

    /**
     * Resets the render queue by clearing all commands.
     */
    void Reset() noexcept;

private:

    /**
     * Represents a command to clear the screen with a specific color.
     *
     * FIELDS:
     * - color: The color used to clear the screen.
     */
    struct ClearCommand {
        Color color;
    };

    /**
     * Represents a command to draw a rectangle with a specific color.
     *
     * FIELDS:
     * - destination: The screen region where the rectangle is drawn.
     * - color: The fill color of the rectangle.
     */
    struct RectangleCommand {
        Rectangle destination;
        Color color;
    };

    /**
     * Represents a command to draw a texture, optionally specifying a source rectangle.
     * If the source rectangle is not provided, the entire texture will be drawn.
     *
     * FIELDS:
     * - texture: The handle of the texture to draw.
     * - source: The optional region of the texture to draw.
     * - destination: The screen region where the texture is drawn.
     */
    struct TextureCommand {
        TextureHandle texture;
        std::optional<Rectangle> source;
        Rectangle destination;
    };

    // union but safe
    using Command = std::variant<ClearCommand, RectangleCommand, TextureCommand>;

    // The backend data structure for storing the rendering commands.
    std::vector<Command> commands;

    // Expose the private members to the RenderQueueExecutor class for execution of the commands.
    friend class internal::RenderQueueExecutor;
};

}

/**
 * Public header file reporting the contract for calling
 * the render queue from external game code.
 * @file render_queue.hpp
 */

#pragma once

#include <svanes/circle_geometry.hpp>
#include <svanes/convex_polygon_geometry.hpp>

#include <svanes/rectangle_geometry.hpp>
#include <svanes/triangle_geometry.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <cstdint>
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
     * @param rotation The rotation angle in radians (default is 0.0F).
     * @param z_order The z order to draw the rectangle at (default is 0).
     */
    void DrawRectangle(Rectangle2D destination, Color color, float rotation = 0.0F, std::int32_t z_order = 0);

    /**
     * Adds a command to draw a triangle to the render queue.
     * @param destination The triangle to be drawn.
     * @param color The color of the triangle.
     * @param z_order The z order to draw the triangle at (default is 0).
     */
    void DrawTriangle(Triangle2D destination, Color color, std::int32_t z_order = 0);

    /**
     * Adds a command to draw a circle to the render queue.
     * 
     * @param destination The circle to be drawn.
     * @param color The color of the circle.
     * @param z_order The z order to draw the circle at (default is 0
     */
    void DrawCircle(Circle2D destination, Color color, std::int32_t z_order = 0);

    /**
     * Adds a command to draw a texture to the render queue.
     * This will draw the entire texture to the specified destination rectangle.
     * @param texture The handle of the texture to draw.
     * @param destination The destination rectangle where the texture will be drawn.
     * @param rotation The rotation angle in radians (default is 0.0F).
     * @param z_order The z order to draw the texture at (default is 0).
     */
    void DrawTexture(TextureHandle texture, Rectangle2D destination, float rotation = 0.0F, std::int32_t z_order = 0);

    /**
     * Adds a command to draw a texture to the render queue with a specified source rectangle.
     * This will draw only the source region of the texture to the specified destination rectangle.
     * @param texture The handle of the texture to draw.
     * @param source The source rectangle from the texture to draw.
     * @param destination The destination rectangle where the texture will be drawn.
     * @param rotation The rotation angle in radians (default is 0.0F).
     * @param z_order The z order to draw the texture at (default is 0).
     */
    void DrawTexture(
        TextureHandle texture, Rectangle2D source, Rectangle2D destination,
        float rotation = 0.0F, std::int32_t z_order = 0
    );

    /**
     * Resets the render queue by clearing all commands.
     */
    void Reset() noexcept;

    void DrawConvexPolygon(const ConvexPolygon2D& destination, Color color, std::int32_t z_order = 0);

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
     * - rotation: The rotation angle in radians for the rectangle.
     * - z_order: The z order the rectangle is drawn at.
     */
    struct RectangleCommand {
        Rectangle2D destination;
        Color color;
        float rotation;
        std::int32_t z_order;
    };

    /**
     * Represents a command to draw a triangle with a specific color.
     *
     * FIELDS:
     * - destination: The triangle to be drawn.
     * - color: The fill color of the triangle.
     * - z_order: The z order the triangle is drawn at.
     */
    struct TriangleCommand {
        Triangle2D destination;
        Color color;
        std::int32_t z_order;
    };

    /**
     * Represents a command to draw a circle with a specific color.
     *
     * FIELDS:
     * - destination: The circle to be drawn.
     * - color: The fill color of the circle.
     * - z_order: The z order the circle is drawn at.
     */
    struct CircleCommand {
        Circle2D destination;
        Color color;
        std::int32_t z_order;
    };

    /**
     * Represents a command to draw a texture, optionally specifying a source rectangle.
     * If the source rectangle is not provided, the entire texture will be drawn.
     *
     * FIELDS:
     * - texture: The handle of the texture to draw.
     * - source: The optional region of the texture to draw.
     * - destination: The screen region where the texture is drawn.
     * - rotation: The rotation angle in radians for the texture.
     * - z_order: The z order the texture is drawn at.
     */
    struct TextureCommand {
        TextureHandle texture;
        std::optional<Rectangle2D> source;
        Rectangle2D destination;
        float rotation;
        std::int32_t z_order;
    };

    /**
     * Represents a command to draw a convex polygon with a specific color.
     * 
     * FIELDS:
     * - destination: The convex polygon to be drawn.
     * - color: The fill color of the convex polygon.
     * - z_order: The z order the convex polygon is drawn at.
     */
    struct ConvexPolygonCommand {
        ConvexPolygon2D destination;
        Color color;
        std::int32_t z_order;
    };

    // union but safe
    using Command = std::variant<ClearCommand, RectangleCommand, TriangleCommand, CircleCommand, ConvexPolygonCommand, TextureCommand>;

    /**
     * Stably sorts the queued commands into ascending z order, so that commands
     * sharing a z order stay in the order they were submitted. A ClearCommand
     * carries no z order of its own and always sorts ahead of every draw.
     */
    void SortByZOrder();

    // The backend data structure for storing the rendering commands.
    std::vector<Command> commands;

    // Expose the private members to the RenderQueueExecutor class for execution of the commands.
    friend class internal::RenderQueueExecutor;
};

}

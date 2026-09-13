# Blend Modes and Radial Gradients

This document records the blend-mode and radial-gradient work currently present on the `shaderPrototype` branch. It is intended as a reimplementation guide when returning to `dev`.

The feature has four layers:

1. Public rendering types and API declarations.
2. Render-queue command storage.
3. ECS component submission through `render_system.cpp`.
4. SDL execution in `render_queue_executor.cpp`.

## Blend Modes

A blend mode controls how a rendered source color combines with the color already present at the same screen location.

For alpha blending, the source alpha acts as opacity. With normalized alpha `a`:

```text
output = source * a + destination * (1 - a)
```

An alpha of zero leaves the destination visible. An alpha of one makes the source fully opaque. Intermediate alpha values produce a mixture.

For additive blending:

```text
output = source * a + destination
```

The destination is not dimmed. This is useful for light and energy effects such as glows, fire, sparks, and laser beams.

The current enum maps to SDL as follows:

```cpp
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
```

The enum belongs in `include/svanes/render/basic_render_types.hpp`.

### Public RenderQueue API

Add `BlendMode blend_mode = BlendMode::Alpha` to each draw API that should support blending. The current declarations are:

```cpp
/**
 * Adds a command to draw a rectangle to the render queue.
 * The rectangle is filled with the specified color and combined with the
 * colors already on the screen according to the selected blend mode.
 *
 * @param destination The destination rectangle.
 * @param color The color of the rectangle.
 * @param rotation The rotation angle in radians (default is 0.0F).
 * @param z_order The z order to draw the rectangle at (default is 0).
 * @param blend_mode The blending mode used to combine the rectangle with
 * the existing screen color (default is BlendMode::Alpha).
 */
void DrawRectangle(Rectangle2D destination, Color color,
                   float rotation = 0.0F, std::int32_t z_order = 0,
                   BlendMode blend_mode = BlendMode::Alpha);

/**
 * Adds a command to draw a triangle to the render queue.
 * The triangle is filled with the specified color and combined with the
 * colors already on the screen according to the selected blend mode.
 *
 * @param destination The triangle to be drawn.
 * @param color The color of the triangle.
 * @param z_order The z order to draw the triangle at (default is 0).
 * @param blend_mode The blending mode used to combine the triangle with the
 * existing screen color (default is BlendMode::Alpha).
 */
void DrawTriangle(Triangle2D destination, Color color,
                  std::int32_t z_order = 0,
                  BlendMode blend_mode = BlendMode::Alpha);

/**
 * Adds a command to draw a circle to the render queue.
 * The circle is filled with the specified color and combined with the
 * colors already on the screen according to the selected blend mode.
 *
 * @param destination The circle to be drawn.
 * @param color The color of the circle.
 * @param z_order The z order to draw the circle at (default is 0).
 * @param blend_mode The blending mode used to combine the circle with the
 * existing screen color (default is BlendMode::Alpha).
 */
void DrawCircle(Circle2D destination, Color color, std::int32_t z_order = 0,
                BlendMode blend_mode = BlendMode::Alpha);

/**
 * Adds a command to draw a texture to the render queue.
 * This will draw the entire texture to the specified destination rectangle.
 * The texture's color and alpha channels are multiplied by the corresponding
 * channels in tint before the texture is combined with the screen.
 *
 * @param texture The handle of the texture to draw.
 * @param destination The destination rectangle where the texture will be drawn.
 * @param rotation The rotation angle in radians (default is 0.0F).
 * @param z_order The z order to draw the texture at (default is 0).
 * @param blend_mode The blending mode used to combine the texture with the
 * existing screen color (default is BlendMode::Alpha).
 * @param tint The color and opacity multiplier applied to the texture.
 *             White with full opacity leaves the texture unchanged.
 *             Other values tint the texture or make it more transparent.
 *             Default is {255, 255, 255, 255}.
 */
void DrawTexture(TextureHandle texture, Rectangle2D destination,
                 float rotation = 0.0F, std::int32_t z_order = 0,
                 BlendMode blend_mode = BlendMode::Alpha,
                 Color tint = {255, 255, 255, 255});

/**
 * Adds a command to draw a texture to the render queue with a specified
 * source rectangle. This will draw only the source region of the texture to
 * the specified destination rectangle. The texture's color and alpha
 * channels are multiplied by the corresponding channels in tint before the
 * texture is combined with the screen.
 *
 * @param texture The handle of the texture to draw.
 * @param source The source rectangle from the texture to draw.
 * @param destination The destination rectangle where the texture will be drawn.
 * @param rotation The rotation angle in radians (default is 0.0F).
 * @param z_order The z order to draw the texture at (default is 0).
 * @param blend_mode The blending mode used to combine the texture with the
 * existing screen color (default is BlendMode::Alpha).
 * @param tint The color and opacity multiplier applied to the texture.
 *             White with full opacity leaves the texture unchanged.
 *             Other values tint the texture or make it more transparent
 *             Default is {255, 255, 255, 255}.
 */
void DrawTexture(TextureHandle texture, Rectangle2D source,
                 Rectangle2D destination, float rotation = 0.0F,
                 std::int32_t z_order = 0,
                 BlendMode blend_mode = BlendMode::Alpha,
                 Color tint = {255, 255, 255, 255});

/**
 * Adds a command to draw a convex polygon to the render queue.
 * The polygon is filled with the specified color and combined with the
 * colors already on the screen according to the selected blend mode.
 *
 * @param destination The convex polygon to be drawn.
 * @param color The color of the polygon.
 * @param z_order The z order to draw the polygon at (default is 0).
 * @param blend_mode The blending mode used to combine the polygon with the
 * existing screen color (default is BlendMode::Alpha).
 */
void DrawConvexPolygon(const ConvexPolygon2D &destination, Color color,
                       std::int32_t z_order = 0,
                       BlendMode blend_mode = BlendMode::Alpha);
```

### Queue storage

The queue stores the mode in each command. The relevant current command definitions are:

```cpp
struct RectangleCommand {
    Rectangle2D destination;
    Color color;
    float rotation;
    std::int32_t z_order;
    BlendMode blend_mode;
};

struct TriangleCommand {
    Triangle2D destination;
    Color color;
    std::int32_t z_order;
    BlendMode blend_mode;
};

struct CircleCommand {
    Circle2D destination;
    Color color;
    Color edge_color;
    std::int32_t z_order;
    BlendMode blend_mode;
};

struct TextureCommand {
    Color tint;
    TextureHandle texture;
    std::optional<Rectangle2D> source;
    Rectangle2D destination;
    float rotation;
    std::int32_t z_order;
    BlendMode blend_mode;
};

struct ConvexPolygonCommand {
    ConvexPolygon2D destination;
    Color color;
    std::int32_t z_order;
    BlendMode blend_mode;
};
```

The queue implementation copies the mode into each command:

```cpp
void RenderQueue::DrawRectangle(Rectangle2D destination, Color color,
                                float rotation, std::int32_t z_order,
                                BlendMode blend_mode) {
    commands.emplace_back(
        RectangleCommand{destination, color, rotation, z_order, blend_mode});
}

void RenderQueue::DrawTriangle(Triangle2D destination, Color color,
                               std::int32_t z_order, BlendMode blend_mode) {
    commands.emplace_back(
        TriangleCommand{destination, color, z_order, blend_mode});
}

void RenderQueue::DrawCircle(Circle2D destination, Color color,
                             std::int32_t z_order, BlendMode blend_mode) {
    commands.emplace_back(
        CircleCommand{destination, color, color, z_order, blend_mode});
}

void RenderQueue::DrawConvexPolygon(const ConvexPolygon2D &destination,
                                    Color color, std::int32_t z_order,
                                    BlendMode blend_mode) {
    commands.emplace_back(
        ConvexPolygonCommand{destination, color, z_order, blend_mode});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle2D destination,
                              float rotation, std::int32_t z_order,
                              BlendMode blend_mode, Color tint) {
    commands.emplace_back(TextureCommand{tint, texture, std::nullopt,
                                         destination, rotation, z_order,
                                         blend_mode});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle2D source,
                              Rectangle2D destination, float rotation,
                              std::int32_t z_order, BlendMode blend_mode,
                              Color tint) {
    commands.emplace_back(TextureCommand{tint, texture, source, destination,
                                         rotation, z_order, blend_mode});
}
```

For a non-gradient circle, `edge_color` is set equal to `color`, so the same executor path can draw both ordinary circles and gradients.

### SDL mapping

The executor maps the public enum to SDL's blend modes and rejects unknown enum values:

```cpp
static SDL_BlendMode ToSDLBlendMode(BlendMode mode) {
    switch (mode) {
    case BlendMode::Alpha:
        return SDL_BLENDMODE_BLEND;
    case BlendMode::Additive:
        return SDL_BLENDMODE_ADD;
    }
    throw std::invalid_argument("Unknown blend mode.");
}
```

Every shape executor sets the renderer blend mode before drawing:

```cpp
if (!SDL_SetRenderDrawBlendMode(renderer,
                                ToSDLBlendMode(command.blend_mode))) {
    throw std::runtime_error("Could not set shape blending: " +
                             std::string{SDL_GetError()});
}
```

The texture executor sets the texture blend mode before drawing:

```cpp
if (!SDL_SetTextureBlendMode(resolved_texture,
                             ToSDLBlendMode(command.blend_mode)) ||
    !SDL_SetTextureColorMod(resolved_texture, command.tint.red,
                            command.tint.green, command.tint.blue) ||
    !SDL_SetTextureAlphaMod(resolved_texture, command.tint.alpha)) {
    throw std::runtime_error("Could not set sprite appearance: " +
                             std::string{SDL_GetError()});
}
```

### ECS components and submission

The render components carry the selected mode:

```cpp
/**
 * Represents a radial gradient to be drawn on the screen.
 * Essentially, this is a circle with a color that transitions
 * from a center color to an edge color, creating a gradient effect.
 *
 * FIELDS:
 * - geometry: The circle defining the area of the gradient.
 * - center_color: The color at the center of the gradient.
 * - edge_color: The color at the edge of the gradient.
 * - blend_mode: The blending mode used to combine the gradient with the
 * existing screen color. Defaults to BlendMode::Alpha.
 */
struct RadialGradient2D {
    Circle2D geometry;
    Color center_color;
    Color edge_color;
    BlendMode blend_mode = BlendMode::Alpha;
};

struct Sprite {
    TextureHandle texture;
    std::optional<Rectangle2D> source;
    Rectangle2D geometry;
    Color tint{255, 255, 255, 255};
    BlendMode blend_mode = BlendMode::Alpha;
};

struct SolidShape {
    Color color;
    Geometry2D geometry;
    BlendMode blend_mode = BlendMode::Alpha;
};
```

The submission code forwards the component mode to the queue:

```cpp
static void SubmitShape(const Rectangle2D &shape, const Transform &transform,
                        Color color, std::int32_t z_order, BlendMode blend_mode,
                        RenderQueue &render_queue, const Camera2D &camera) {
    const auto destination = camera.PrepareForRendering(transform, shape);

    if (!destination) {
        return;
    }

    render_queue.DrawRectangle(*destination, color, transform.rotation, z_order,
                               blend_mode);
}

static void SubmitShape(const Triangle2D &shape, const Transform &transform,
                        Color color, std::int32_t z_order, BlendMode blend_mode,
                        RenderQueue &render_queue, const Camera2D &camera) {
    const auto destination = camera.PrepareForRendering(transform, shape);

    if (!destination) {
        return;
    }

    render_queue.DrawTriangle(*destination, color, z_order, blend_mode);
}

static void SubmitShape(const Circle2D &shape, const Transform &transform,
                        Color color, std::int32_t z_order, BlendMode blend_mode,
                        RenderQueue &render_queue, const Camera2D &camera) {
    const auto destination = camera.PrepareForRendering(transform, shape);

    if (!destination) {
        return;
    }

    render_queue.DrawCircle(*destination, color, z_order, blend_mode);
}

static void SubmitShape(const ConvexPolygon2D &shape,
                        const Transform &transform, Color color,
                        std::int32_t z_order, BlendMode blend_mode,
                        RenderQueue &render_queue, const Camera2D &camera) {
    const auto destination = camera.PrepareForRendering(transform, shape);
    if (!destination) {
        return;
    }
    render_queue.DrawConvexPolygon(*destination, color, z_order, blend_mode);
}
```

The component iteration forwards `SolidShape::blend_mode` into the variant visitor:

```cpp
std::visit(
    [&](const auto &shape) {
        SubmitShape(shape, transform, visual.color, z_order,
                    visual.blend_mode, render_queue, camera);
    },
    visual.geometry);
```

Sprite submission forwards both blend mode and tint:

```cpp
if (sprite.source.has_value()) {
    render_queue.DrawTexture(sprite.texture, *sprite.source,
                             *destination, transform.rotation,
                             z_order, sprite.blend_mode,
                             sprite.tint);
} else {
    render_queue.DrawTexture(sprite.texture, *destination,
                             transform.rotation, z_order,
                             sprite.blend_mode, sprite.tint);
}
```

## Radial Gradients

A radial gradient is implemented as a triangle fan. The center vertex uses `center_color`, and all circumference vertices use `edge_color`. SDL interpolates vertex colors across each triangle, producing the gradient between the center and edge.

The public queue declaration is:

```cpp
/**
 * Adds a command to draw a circle whose color changes gradually from its
 * center to its circumference.
 *
 * @param destination The circle to be drawn.
 * @param center_color The color at the center of the circle.
 * @param edge_color The color at the circumference of the circle.
 * @param z_order The z order to draw the gradient at (default is 0).
 * @param blend_mode The blending mode used to combine the gradient with the
 * existing screen color (default is BlendMode::Alpha).
 */
void DrawRadialGradient(Circle2D destination, Color center_color,
                        Color edge_color, std::int32_t z_order = 0,
                        BlendMode blend_mode = BlendMode::Alpha);
```

The component submission is:

```cpp
void SubmitRadialGradients(const Registry &world, RenderQueue &render_queue,
                           const Camera2D &camera) {
    world.ForEach<Transform, RadialGradient2D>(
        [&](Entity entity, const Transform &transform,
            const RadialGradient2D &gradient) {
            const auto destination =
                camera.PrepareForRendering(transform, gradient.geometry);
            if (destination) {
                render_queue.DrawRadialGradient(
                    *destination, gradient.center_color, gradient.edge_color,
                    ZOrderOf(world, entity), gradient.blend_mode);
            }
        });
}
```

The queue implementation is:

```cpp
void RenderQueue::DrawRadialGradient(Circle2D destination, Color center_color,
                                     Color edge_color, std::int32_t z_order,
                                     BlendMode blend_mode) {
    commands.emplace_back(CircleCommand{destination, center_color, edge_color,
                                        z_order, blend_mode});
}
```

The circle executor creates the fan. The first vertex is at the circle center, and the remaining 64 vertices are evenly distributed around the circumference:

```cpp
if (!SDL_SetRenderDrawBlendMode(renderer,
                                ToSDLBlendMode(command.blend_mode))) {
    throw std::runtime_error("Could not set shape blending: " +
                             std::string{SDL_GetError()});
}

constexpr std::int32_t segments = 64;

const SDL_FColor color{
    command.color.red / 255.0F,
    command.color.green / 255.0F,
    command.color.blue / 255.0F,
    command.color.alpha / 255.0F,
};

const SDL_FColor edge_color{
    command.edge_color.red / 255.0F,
    command.edge_color.green / 255.0F,
    command.edge_color.blue / 255.0F,
    command.edge_color.alpha / 255.0F,
};

std::array<SDL_Vertex, segments + 1> vertices{};
std::array<std::int32_t, segments * 3> indices{};

vertices[0] = {{command.destination.x, command.destination.y}, color, {}};

for (std::int32_t i = 0; i < segments; ++i) {
    const float angle = 2.0F * std::numbers::pi_v<float> * i / segments;

    vertices[i + 1] = {{
                           command.destination.x +
                               command.destination.radius * std::cos(angle),
                           command.destination.y +
                               command.destination.radius * std::sin(angle),
                       },
                       edge_color,
                       {}};

    indices[i * 3] = 0;
    indices[i * 3 + 1] = i + 1;
    indices[i * 3 + 2] = (i + 1) % segments + 1;
}

if (!SDL_RenderGeometry(renderer, nullptr, vertices.data(), segments + 1,
                        indices.data(), segments * 3)) {
    throw std::runtime_error("Could not draw a circle: " +
                             std::string{SDL_GetError()});
}
```

The full current executor also includes explanatory comments:

```cpp
// How many triangles we should use in a fan to approximate the circle.
constexpr std::int32_t segments = 64;

// Need 3 indices per triangle, and we have segments many triangles in the fan.
std::array<std::int32_t, segments * 3> indices{};

// The first vertex is the center of the circle, and the rest are the points on the circumference.
vertices[0] = {{command.destination.x, command.destination.y}, color, {}};

// For each triangle, we need to calculate the angle and the corresponding point on the circumference
// of the circle. Then we set up indices so that each triple of indices[i*3], indices[i*3 + 1], indices[i*3 + 2]
// forms a triangle in the fan. The first index is always 0 (the center), and the other two are the current point
// and the next point (wrapping around to the first point after the last).
for (std::int32_t i = 0; i < segments; ++i) {
    // 2 pi * i / segments gives us the angle in radians for the current segment.
    const float angle = 2.0F * std::numbers::pi_v<float> * i / segments;

    // Center
    indices[i * 3] = 0;

    // Current point on the circumference
    indices[i * 3 + 1] = i + 1;

    // Next point on the circumference, wrapping around to the first point after the last.
    indices[i * 3 + 2] = (i + 1) % segments + 1;
}
```

### Radial-gradient data flow

```text
RadialGradient2D component
    -> SubmitRadialGradients
    -> RenderQueue::DrawRadialGradient
    -> CircleCommand { center color, edge color, blend mode }
    -> RenderQueueExecutor::Execute(CircleCommand)
    -> SDL triangle fan with interpolated vertex colors
```

### Reimplementation checklist

- Add `BlendMode` to `basic_render_types.hpp`.
- Add `blend_mode` parameters to the public shape and texture queue methods.
- Add `blend_mode` fields to every relevant queue command.
- Add `edge_color` to `CircleCommand`.
- Add `DrawRadialGradient` to the public queue API and implementation.
- Add `RadialGradient2D` and its submission function to `render_system.hpp/.cpp`.
- Forward `SolidShape::blend_mode` through every shape submission overload.
- Forward `Sprite::blend_mode` and `Sprite::tint` through both texture overloads.
- Map `BlendMode::Alpha` to `SDL_BLENDMODE_BLEND`.
- Map `BlendMode::Additive` to `SDL_BLENDMODE_ADD`.
- Set the renderer blend mode before shape and geometry draws.
- Set the texture blend mode, color modulation, and alpha modulation before texture draws.
- Build the radial gradient as a 64-segment triangle fan.
- Use the center color for the center vertex and the edge color for circumference vertices.
- Keep the default blend mode as `BlendMode::Alpha`.
- Preserve stable z-order sorting and existing queue variant handling.

## Current branch notes

The snippets above are copied from the current `shaderPrototype` implementation. The surrounding branch also contains separate bloom/HDR work; this document intentionally records only the blend-mode and radial-gradient pieces needed to reimplement those features.

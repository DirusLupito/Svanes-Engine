/**
 * Implements the RenderQueueExecutor class, 
 * which is responsible for executing rendering commands
 * stored in a RenderQueue.
 * @file render_queue_executor.cpp
 */

#include "render_queue_executor.hpp"

#include "texture_manager_internal.hpp"

#include <svanes/rectangle_geometry.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>

namespace svanes::internal {

RenderQueueExecutor::RenderQueueExecutor(
    SDL_Renderer* renderer,
    const TextureManager& texture_manager
)
    : renderer(renderer),
      texture_manager(texture_manager)
{
    if (renderer == nullptr) {
        throw std::invalid_argument("Render queue executor requires a renderer.");
    }
}

void RenderQueueExecutor::Execute(RenderQueue& render_queue) const
{
    render_queue.SortByZOrder();

    // Iterate through each command and cast it to the appropriate type, then execute it.
    for (const auto& command : render_queue.commands) {
        if (const auto* clear = std::get_if<RenderQueue::ClearCommand>(&command)) {
            Execute(*clear);
            continue;
        }

        if (const auto* rectangle = std::get_if<RenderQueue::RectangleCommand>(&command)) {
            Execute(*rectangle);
            continue;
        }

        if (const auto* triangle = std::get_if<RenderQueue::TriangleCommand>(&command)) {
            Execute(*triangle);
            continue;
        }

        if (const auto* circle = std::get_if<RenderQueue::CircleCommand>(&command)) {
            Execute(*circle);
            continue;
        }

        Execute(std::get<RenderQueue::TextureCommand>(command));
    }
}

void RenderQueueExecutor::Execute(const RenderQueue::ClearCommand& command) const
{
    // Set the draw color, then delegate to SDL_RenderClear to cleanse the renderer.
    SetDrawColor(command.color);
    if (!SDL_RenderClear(renderer)) {
        throw std::runtime_error("Could not clear the renderer: " + std::string{SDL_GetError()});
    }
}

void RenderQueueExecutor::Execute(const RenderQueue::RectangleCommand& command) const
{
    SetDrawColor(command.color);

    // Convert center-based rectangle coordinates to SDL's top-left-based rectangle coordinates.

    const SDL_FRect destination{
        command.destination.x - command.destination.width * 0.5F,
        command.destination.y - command.destination.height * 0.5F,
        command.destination.width,
        command.destination.height,
    };

    // If the rectangle has a non-zero rotation, 
    // we need draw it instead using SDL_RenderGeometry
    // and calculate the four corners/vertices of the rectangle after rotation.

    if (command.rotation != 0.0F) {
        // SDL_RenderGeometry requires the color to be specified
        // as a floating-point value in the range [0.0, 1.0]

        const SDL_FColor color{
            command.color.red / 255.0F,
            command.color.green / 255.0F,
            command.color.blue / 255.0F,
            command.color.alpha / 255.0F,
        };

        const auto corners = RectangleGeometry(command.destination, command.rotation).Corners();
        SDL_Vertex vertices[4]{};
        for (std::size_t i = 0; i < corners.size(); ++i) {
            vertices[i] = {{corners[i].x, corners[i].y}, color, {}};
        }

        // Now we draw it as two triangles. In SDL_RenderGeometry,
        // every three vertices are considered a triangle. 

        // Triangle 1: Top right, Bottom right, Bottom left
        // Triangle 2: Bottom left, Top left, Top right
        constexpr std::int32_t indices[]{1, 2, 3, 3, 0, 1,};

        if (!SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6)) {
            throw std::runtime_error("Could not draw a rotated rectangle: " + std::string{SDL_GetError()});
        }
    } else if (!SDL_RenderFillRect(renderer, &destination)) {
        throw std::runtime_error("Could not draw a rectangle: " + std::string{SDL_GetError()});
    }
}

void RenderQueueExecutor::Execute(const RenderQueue::TriangleCommand& command) const
{
    const SDL_FColor color{
        command.color.red / 255.0F,
        command.color.green / 255.0F,
        command.color.blue / 255.0F,
        command.color.alpha / 255.0F,
    };

    SDL_Vertex vertices[3]{};

    for (std::size_t i = 0; i < command.destination.vertices.size(); ++i) {
        const Vector2D point = command.destination.vertices[i];
        vertices[i] = {{point.x, point.y}, color, {}};
    }

    if (!SDL_RenderGeometry(renderer, nullptr, vertices, 3, nullptr, 0)) {
        throw std::runtime_error("Could not draw a triangle: " + std::string{SDL_GetError()});
    }
}

void RenderQueueExecutor::Execute(const RenderQueue::CircleCommand& command) const
{
    // How many triangles we should use in a fan to approximate the circle. 
    constexpr std::int32_t segments = 64;

    const SDL_FColor color{
        command.color.red / 255.0F,
        command.color.green / 255.0F,
        command.color.blue / 255.0F,
        command.color.alpha / 255.0F,
    };

    std::array<SDL_Vertex, segments + 1> vertices{};

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

        vertices[i + 1] = {{
            command.destination.x + command.destination.radius * std::cos(angle),
            command.destination.y + command.destination.radius * std::sin(angle),
        }, color, {}};

        // Center
        indices[i * 3] = 0;

        // Current point on the circumference
        indices[i * 3 + 1] = i + 1;

        // Next point on the circumference, wrapping around to the first point after the last.
        indices[i * 3 + 2] = (i + 1) % segments + 1;
    }

    if (!SDL_RenderGeometry(renderer, nullptr, vertices.data(), segments + 1, indices.data(), segments * 3)) {
        throw std::runtime_error("Could not draw a circle: " + std::string{SDL_GetError()});
    }
}

void RenderQueueExecutor::Execute(const RenderQueue::TextureCommand& command) const
{
    // Figure out which texture the handle is referring to.

    SDL_Texture* resolved_texture = TextureManagerInternal::Resolve(texture_manager, command.texture);

    // Convert center-based rectangle coordinates to SDL's top-left-based rectangle coordinates.

    const SDL_FRect destination{
        command.destination.x - command.destination.width * 0.5F,
        command.destination.y - command.destination.height * 0.5F,
        command.destination.width,
        command.destination.height,
    };

    SDL_FRect source{};

    // If we have a source rectangle, this is it.
    const SDL_FRect* source_pointer = nullptr;

    if (command.source.has_value()) {
        source = SDL_FRect{
            command.source->x - command.source->width * 0.5F,
            command.source->y - command.source->height * 0.5F,
            command.source->width,
            command.source->height,
        };
        source_pointer = &source;
    }

    const double rotation_degrees = static_cast<double>(command.rotation) * 180.0 / std::numbers::pi;
    if (!SDL_RenderTextureRotated(
        renderer, resolved_texture, source_pointer, &destination, rotation_degrees, nullptr, SDL_FLIP_NONE
    )) {
        throw std::runtime_error("Could not draw a texture: " + std::string{SDL_GetError()});
    }
}

void RenderQueueExecutor::SetDrawColor(Color color) const
{
    if (!SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha)) {
        throw std::runtime_error("Could not set the render draw color: " + std::string{SDL_GetError()});
    }
}

}

/**
 * Implements the RenderQueueExecutor class, 
 * which is responsible for executing rendering commands
 * stored in a RenderQueue.
 * @file render_queue_executor.cpp
 */

#include "render_queue_executor.hpp"

#include "texture_manager_internal.hpp"

#include <SDL3/SDL.h>

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

void RenderQueueExecutor::Execute(const RenderQueue& render_queue) const
{
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
    const SDL_FRect destination{
        command.destination.x,
        command.destination.y,
        command.destination.width,
        command.destination.height,
    };

    // If the rectangle has a non-zero rotation, 
    // we need draw it instead using SDL_RenderGeometry
    // and calculate the four vertices of the rectangle after rotation.

    if (command.rotation != 0.0F) {
        const float half_width = destination.w * 0.5F;
        const float half_height = destination.h * 0.5F;
        const float center_x = destination.x + half_width;
        const float center_y = destination.y + half_height;
        const float cosine = std::cos(command.rotation);
        const float sine = std::sin(command.rotation);

        // SDL_RenderGeometry requires the color to be specified
        // as a floating-point value in the range [0.0, 1.0]

        const SDL_FColor color{
            command.color.red / 255.0F,
            command.color.green / 255.0F,
            command.color.blue / 255.0F,
            command.color.alpha / 255.0F,
        };

        // The rectangle's center stays in the same place
        // at (center_x, center_y) while the four corners
        // are rotated around that center point.

        // We initialize the vertices as relative positions
        // to the center of the rectangle.

        SDL_Vertex vertices[]{
            // Top left
            {{-half_width, -half_height}, color, {}},

            // Top right
            {{half_width, -half_height}, color, {}},

            // Bottom right
            {{half_width, half_height}, color, {}},

            // Bottom left
            {{-half_width, half_height}, color, {}},
        };

        for (SDL_Vertex& vertex : vertices) {
            const SDL_FPoint offset = vertex.position;

            // Given our center point (center_x, center_y), the angle of a given
            // vertex from the center will be given by atan2(offset.y, offset.x)

            // We can then just add the rotation to that angle, which gives us
            // the new angle of the vertex from the center.

            // Take     phi   = atan2(offset.y, offset.x)
            // and take theta = command.rotation
            // Then the new position of the vertex will be given by:

            // x' = r * cos(phi + theta)
            // y' = r * sin(phi + theta)

            // Where r = sqrt(offset.x^2 + offset.y^2) is the distance from the center.
            // Recall:
            // cos(x + y) = cos(x)cos(y) - sin(x)sin(y)
            // sin(x + y) = sin(x)cos(y) + cos(x)sin(y)
            // x = r * cos(atan2(y, x))
            // y = r * sin(atan2(y, x)).

            // So we have
            // x' = r * cos(phi + theta)
            //    = r * (cos(phi)cos(theta) - sin(phi)sin(theta))
            //    = x * cos(theta) - y * sin(theta)

            // y' = r * sin(phi + theta)
            //    = r * (sin(phi)cos(theta) + cos(phi)sin(theta))
            //    = x * sin(theta) + y * cos(theta)

            // We can then just add the center point back to get the final position of the vertex.

            vertex.position = {
                center_x + offset.x * cosine - offset.y * sine,
                center_y + offset.x * sine + offset.y * cosine,
            };
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

void RenderQueueExecutor::Execute(const RenderQueue::TextureCommand& command) const
{
    // Figure out which texture the handle is referring to.

    SDL_Texture* resolved_texture = TextureManagerInternal::Resolve(texture_manager, command.texture);

    const SDL_FRect destination{
        command.destination.x,
        command.destination.y,
        command.destination.width,
        command.destination.height,
    };

    SDL_FRect source{};

    // If we have a source rectangle, this is it.
    const SDL_FRect* source_pointer = nullptr;

    if (command.source.has_value()) {
        source = SDL_FRect{
            command.source->x,
            command.source->y,
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

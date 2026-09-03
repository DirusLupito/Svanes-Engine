/**
 * Implements the RenderQueueExecutor class, 
 * which is responsible for executing rendering commands
 * stored in a RenderQueue.
 * @file render_queue_executor.cpp
 */

#include "render_queue_executor.hpp"

#include "texture_manager_internal.hpp"

#include <SDL3/SDL.h>

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

    if (!SDL_RenderFillRect(renderer, &destination)) {
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

    if (!SDL_RenderTexture(renderer, resolved_texture, source_pointer, &destination)) {
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

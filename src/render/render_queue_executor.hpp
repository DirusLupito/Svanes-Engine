/**
 * Internal interface for executing rendering commands
 * stored in a RenderQueue. 
 * @file render_queue_executor.hpp
 */

#pragma once

#include <svanes/render/render_queue.hpp>
#include <svanes/render/texture_manager.hpp>

struct SDL_Renderer;

namespace svanes::internal {

/**
 * Executes rendering commands stored in a RenderQueue.
 */
class RenderQueueExecutor final {
public:

    /**
     * Constructs a RenderQueueExecutor with the specified SDL_Renderer and TextureManager.
     * @param renderer The SDL_Renderer used for rendering.
     * @param texture_manager The TextureManager used for managing textures.
     * @throws std::invalid_argument if the renderer is null.
     */
    RenderQueueExecutor(SDL_Renderer* renderer, const TextureManager& texture_manager);

    /**
     * Execute the rendering commands stored in the provided RenderQueue.
     * @param render_queue The RenderQueue containing the commands to execute.
     * @throws std::runtime_error if any command fails to execute.
     */
    void Execute(const RenderQueue& render_queue) const;

private:
    /**
     * Executes a single ClearCommand, which clears the screen with a specific color.
     * @param command The ClearCommand to execute.
     * @throws std::runtime_error if the renderer cannot be cleared.
     */
    void Execute(const RenderQueue::ClearCommand& command) const;

    /**
     * Executes a single RectangleCommand, which draws a rectangle with a specific color.
     * @param command The RectangleCommand to execute.
     * @throws std::runtime_error if the rectangle cannot be drawn.
     */
    void Execute(const RenderQueue::RectangleCommand& command) const;

    /**
     * Executes a single TextureCommand, which draws a texture, 
     * optionally specifying a source rectangle.
     * @param command The TextureCommand to execute.
     * @throws std::runtime_error if the texture cannot be drawn.
     */
    void Execute(const RenderQueue::TextureCommand& command) const;

    /**
     * Sets the draw color for the SDL_Renderer based on the provided Color.
     * @param color The Color to set as the draw color.
     * @throws std::runtime_error if the draw color cannot be set.
     */
    void SetDrawColor(Color color) const;

    /**
     * The SDL_Renderer used for rendering.
     * This will handle the actual drawing operations to the screen.
     */
    SDL_Renderer* renderer;

    /**
     * The TextureManager used for managing textures.
     * This will hold the datastructure containing
     * all the textures and their corresponding handles.
     */
    const TextureManager& texture_manager;
};

}

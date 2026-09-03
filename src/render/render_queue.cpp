/**
 * Implements the RenderQueue class, 
 * which is responsible for managing a queue
 * of rendering commands to be executed.
 */

#include <svanes/render/render_queue.hpp>

namespace svanes {

// Currently all the commands simply add the command

void RenderQueue::Clear(Color color)
{
    commands.emplace_back(ClearCommand{color});
}

void RenderQueue::DrawRectangle(Rectangle destination, Color color)
{
    commands.emplace_back(RectangleCommand{destination, color});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle destination)
{
    commands.emplace_back(TextureCommand{texture, std::nullopt, destination});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle source, Rectangle destination)
{
    commands.emplace_back(TextureCommand{texture, source, destination});
}

// Except for reset which just clears the command queue

void RenderQueue::Reset() noexcept
{
    commands.clear();
}

}

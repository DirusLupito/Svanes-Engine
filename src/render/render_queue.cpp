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

void RenderQueue::DrawRectangle(Rectangle2D destination, Color color, float rotation)
{
    commands.emplace_back(RectangleCommand{destination, color, rotation});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle2D destination, float rotation)
{
    commands.emplace_back(TextureCommand{texture, std::nullopt, destination, rotation});
}

void RenderQueue::DrawTexture(TextureHandle texture, Rectangle2D source, Rectangle2D destination, float rotation)
{
    commands.emplace_back(TextureCommand{texture, source, destination, rotation});
}

// Except for reset which just clears the command queue

void RenderQueue::Reset() noexcept
{
    commands.clear();
}

}

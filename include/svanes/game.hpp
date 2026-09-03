#pragma once

#include <cstdint>

namespace svanes {

class InputManager;
class RenderQueue;
class TextureManager;

/**
 * Defines relevant context for a game. 
 * For games to have their textures managed by the engine,
 * they must use the TextureManager provided in this context.
 */
struct GameContext {
    TextureManager& assets;
};

/**
 * Defines relevant context for a single frame of a game.
 * For games to have their input managed by the engine,
 * they must use the InputManager provided in this context.
 * Furthermore, the engine fills in the delta_seconds field
 * with the time elapsed since the last frame, allowing the game
 * to update its state accordingly.
 */
struct FrameContext {
    const InputManager& input;
    float delta_seconds;
};

/**
 * Defines the context used to build one frame's render queue.
 *
 * FIELDS:
 * - render_queue: The render queue that receives commands for the current frame.
 * - output_width: The current render output width in pixels.
 * - output_height: The current render output height in pixels.
 */
struct RenderContext {
    RenderQueue& render_queue;
    std::int32_t output_width;
    std::int32_t output_height;
};

/**
 * Any game that is to be run by the engine must implement this interface.
 * It provides a bridge between the engine and the game, allowing the engine
 * to manage the game loop and rendering while the game implements its own logic.
 */
class IGame {
public:
    virtual ~IGame() = default;

    /**
     * Initializes the game with the provided context.
     * This method is called once at the start of the game.
     * @param context The context for the game, providing access to the TextureManager.
     */
    virtual void Initialize(GameContext& context) = 0;

    /**
     * Updates the game state based on the provided frame context.
     * This method is called once per frame, allowing the game to process input and update its state.
     * @param frame The context for the current frame, providing access to the 
     * InputManager and the time elapsed since the last frame.
     */
    virtual void Update(const FrameContext& frame) = 0;

    /**
     * Builds the render queue for the current frame.
     * This method is called once per frame, allowing the game to specify what should be rendered.
     * @param context The context containing the render queue and current output dimensions.
     */
    virtual void BuildRenderQueue(const RenderContext& context) = 0;

    /**
     * Determines whether the game should quit on the next
     * iteration of the game loop. 
     * @return True if the game should quit, false otherwise.
     */
    virtual bool ShouldQuit() const { return false; }
};

} // namespace svanes

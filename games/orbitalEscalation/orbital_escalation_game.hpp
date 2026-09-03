#pragma once

#include <svanes/game.hpp>

/**
 * Top level container for the Orbital Escalation game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class OrbitalEscalationGame final : public svanes::IGame {
public:
    ~OrbitalEscalationGame() override;

    // These 4 methods must be called to implement the engine bridge.

    /**
     * Initializes the game with the provided context.
     * @param context The context for the game, providing access to the TextureManager.
     */
    void Initialize(svanes::GameContext& context) override;

    /**
     * Game specific update logic. Called by the engine once per frame.
     * @param frame The context for the current frame, providing access to the InputManager
     * and the time elapsed since the last frame.
     */
    void Update(const svanes::FrameContext& frame) override;

    /**
     * Builds the render queue for the current frame. Called by the engine once per frame.
     * @param render_queue The render queue to which the game should add its renderable objects
     */
    void BuildRenderQueue(svanes::RenderQueue& render_queue) override;

    /**
     * Renders the current frame using the provided SDL_Renderer. Called by the engine once per frame.
     * @param renderer The SDL_Renderer to use for rendering.
     * @param output_width The width of the output window.
     * @param output_height The height of the output window.
     */
    void OnRender(SDL_Renderer* renderer, int32_t output_width, int32_t output_height) override;

private:
    // Total time elapsed since the start of the game, in seconds.
    float elapsed_seconds = 0.0F;
    SDL_Texture* gradient_texture = nullptr;
};

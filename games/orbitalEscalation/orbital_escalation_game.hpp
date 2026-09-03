#pragma once

#include <svanes/game.hpp>
#include <svanes/render/basic_render_types.hpp>

/**
 * Top level container for the Orbital Escalation game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class OrbitalEscalationGame final : public svanes::IGame {
public:
    // These 3 methods must be called to implement the engine bridge.

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
    void BuildRenderQueue(const svanes::RenderContext& context) override;

private:
    // Total time elapsed since the start of the game, in seconds.
    float elapsed_seconds = 0.0F;
    svanes::TextureHandle gradient_texture;
};

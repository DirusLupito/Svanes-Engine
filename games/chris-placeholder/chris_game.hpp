#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/render/basic_render_types.hpp>

/**
 * Top level container for Chris's placeholder game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class ChrisGame final : public svanes::IGame {
public:
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

private:
    // Total time elapsed since the start of the game, in seconds.
    float elapsed_seconds = 0.0F;
    svanes::Entity background_entity = 0;
    svanes::Entity square_entity = 0;
    svanes::Entity character_entity = 0;
    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle running_texture{};
    bool is_running = false;
};

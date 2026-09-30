#pragma once

#include "orbital_simulation.hpp"

#include <svanes/entity.hpp>
#include <svanes/game.hpp>

#include <optional>

/**
 * Top level container for the Orbital Escalation game.
 * Used to hold bridge components responsible for talking
 * to the engine.
 */
class OrbitalEscalationGame final : public svanes::IGame {
public:
    /**
     * Initializes the game with the provided context.
     * @param context The context for the game, providing access to the
     * TextureManager.
     */
    void Initialize(svanes::GameContext &context) override;

    /**
     * Game specific update logic. Called by the engine once per frame.
     * @param frame The context for the current frame, providing access to the
     * InputManager and the time elapsed since the last frame.
     */
    void Update(const svanes::FrameContext &frame) override;

    /**
     * Checks whether the player has requested to quit the game.
     *
     * @return true if the game should close, false otherwise.
     */
    bool ShouldQuit() const override;

private:
    // Owns and advances the gameplay world, independently of rendering.
    std::unique_ptr<OrbitalSimulation> simulation;

    // The timeline used to animate the pause label.
    svanes::Timeline pause_timeline;

    // Real time accumulated toward the next complete simulation step.
    svanes::TicCount pending_tics = 0;

    // The text label displayed while gameplay is paused.
    svanes::Entity pause_label_entity = 0;

    // The background rectangle that follows the camera's view.
    svanes::Entity background_entity = 0;

    // Whether the player has requested to close the game.
    bool should_quit = false;
};

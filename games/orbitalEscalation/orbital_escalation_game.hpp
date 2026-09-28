#pragma once

#include "planet.hpp"
#include "ship.hpp"

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
     * Post-physics update logic. Called by the engine once per simulation step
     * immediately after the physics system has updated all entities' for this
     * one single simulation step. This runs at most once per rendered frame,
     * before animation and Update, and is skipped when no step is due.
     *
     * @param physics The world, sampled input, and entities' local tic deltas.
     */
    void PhysicsUpdate(const svanes::PhysicsContext &physics) override;

    bool ShouldQuit() const override;

private:
    // The overarching timeline entity for which all other timelines are
    // children.
    svanes::Entity gameplay_timeline_entity = 0;

    svanes::Entity pause_timeline_entity = 0;
    svanes::Entity pause_label_entity = 0;


    svanes::Entity background_entity = 0;
    std::optional<Ship> player_ship;
    std::optional<Planet> planet;

    bool should_quit = false;
};

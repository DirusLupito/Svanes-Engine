#pragma once

#include <svanes/entity.hpp>
#include <svanes/game.hpp>

#include "enemy.hpp"
#include "goose.hpp"

class ErikGame final : public svanes::IGame {
public:
    /**
     * Builds the starting world: gravity, the background, the static geometry, and
     * the entities the game begins with.
     *
     * @param context The game context supplying the registry, texture loader, and
     * the world gravity vector.
     */
    void Initialize(svanes::GameContext& context) override;

    /**
     * Advances the game by one frame.
     *
     * @param frame The frame context supplying the registry, input, camera, frame
     * delta, and output size.
     */
    void Update(const svanes::FrameContext& frame) override;

    /**
     * @return Whether the application should stop running.
     */
    bool ShouldQuit() const override;

private:
    svanes::Entity orb{};

    // the sky, kept centered on the camera and sized to cover the view every frame
    svanes::Entity background{};

    Enemy enemy;
    Goose goose;

    // total time the game has been running, used to place the enemy along its path
    float elapsed_seconds = 0.0F;

    // time left to press A or D a second time and turn it into a dash, counted
    // down separately per direction so a left tap cannot complete a right dash
    float left_tap_timer = 0.0F;
    float right_tap_timer = 0.0F;

    bool should_quit = false;
};

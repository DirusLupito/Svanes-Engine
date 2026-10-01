#pragma once

#include "goose_session.hpp"

#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * The menu Escape opens and closes. Q or the Quit button asks to leave.
 *
 * It has two rows of three options. The game speed row (0.5x, 1x, 2x) works
 * only while the local player is alone. The loop rate row (30, 60, 120 frames
 * per second) works only on a goose server, where it changes how often this
 * client runs its loop and sends input, without affecting anyone else.
 *
 * The applied option of each row is filled in, and a border marks the
 * highlighted one. Up and Down, or W and S, move between enabled rows. Left
 * and Right, or A and D, move the highlight, and Enter applies it. Clicking an
 * option highlights and applies it at once. Closing the menu forgets a
 * highlight that was not applied. A disabled row is greyed out and ignores input.
 *
 * A label in the corner of the screen shows any setting that is not 1x.
 */
class PauseMenu final {
public:
    /**
     * Creates the menu's shapes and labels, hidden until the menu opens.
     * @param context The world to create them in and the fonts to draw with.
     * @param on_server Whether this process plays on a goose server, which enables the loop rate row.
     * @throws std::runtime_error if the menu font cannot be loaded.
     */
    PauseMenu(svanes::GameContext& context, bool on_server);

    /**
     * Opens or closes the menu on Escape and, while it is open, handles its keys and clicks.
     * @param frame The frame's input and camera.
     * @param alone Whether the local player is the only one in the world.
     */
    void HandleInput(const svanes::FrameContext& frame, bool alone);

    /**
     * Places the menu and corner label over the current view. Call after the camera has moved.
     * @param frame The frame's world and camera.
     * @param alone Whether the local player is the only one in the world.
     */
    void Draw(const svanes::FrameContext& frame, bool alone);

    /** @return Whether the menu is showing. */
    bool IsOpen() const;

    /**
     * Reports a quit request once.
     * @return Whether the player asked to quit since the last call.
     */
    bool TakeQuitRequest();

    /** @return The applied game speed. */
    GooseSpeed Speed() const;

    /** @return The applied loop rate, as a multiple of 60 frames per second. */
    GooseSpeed LoopRate() const;

    /** Returns the game speed to 1x, as when another player joins. */
    void ResetSpeed();

private:
    /**
     * A rectangle of the menu with text centered on it.
     * FIELDS:
     * - area: The rectangle's center and size, in pixels from the viewport's top-left corner.
     * - shape: The entity drawing the rectangle.
     * - label: The entity drawing the text.
     */
    struct Button {
        svanes::Rectangle2D area;
        svanes::Entity shape;
        svanes::Entity label;
    };

    /**
     * A captioned row of three options.
     * FIELDS:
     * - buttons: The options, slowest first.
     * - caption: The entity drawing the row's title.
     * - applied: The option in effect.
     */
    struct Row {
        std::array<Button, 3> buttons;
        svanes::Entity caption;
        GooseSpeed applied = GooseSpeed::Normal;
    };

    /**
     * @param row The row's index.
     * @param alone Whether the local player is the only one in the world.
     * @return Whether the row accepts input.
     */
    bool IsEnabled(std::size_t row, bool alone) const;

    /**
     * Moves focus to an enabled row and its applied option, if the focused row is disabled.
     * @param alone Whether the local player is the only one in the world.
     */
    void KeepFocusEnabled(bool alone);

    /**
     * Moves the menu's areas so the menu stays centered in the viewport.
     * @param viewport The viewport in screen coordinates.
     */
    void Layout(const svanes::Rectangle2D& viewport);

    bool on_server;
    bool open = false;
    bool quit_requested = false;
    std::size_t focused_row = 0;
    GooseSpeed highlighted = GooseSpeed::Normal;

    svanes::Rectangle2D backdrop_area{};
    svanes::Entity backdrop{};
    svanes::Entity highlight{};
    std::array<Row, 2> rows{};
    Button quit_button{};
    svanes::Entity title{};
    svanes::Entity note{};
    svanes::Entity hint{};
    svanes::Entity corner_label{};
};

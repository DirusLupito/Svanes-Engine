#pragma once

#include "goose_session.hpp"

#include <svanes/entity.hpp>
#include <svanes/game.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>

#include <array>
#include <cstdint>

/**
 * The menu Escape opens and closes. Q or the Quit button asks to leave.
 *
 * While the local player is alone, the speed row chooses how fast the game
 * runs. The applied speed is filled in and a border marks the highlighted one.
 * Left and Right, or A and D, move the highlight and Enter applies it.
 * Clicking an option highlights and applies it at once. Closing the menu forgets a highlight that was not
 * applied. With other players present the row is greyed out and ignores input.
 *
 * A label in the corner of the screen shows the speed whenever it is not 1x.
 */
class PauseMenu final {
public:
    /**
     * Creates the menu's shapes and labels, hidden until the menu opens.
     * @param context The world to create them in and the fonts to draw with.
     * @throws std::runtime_error if the menu font cannot be loaded.
     */
    explicit PauseMenu(svanes::GameContext& context);

    /**
     * Opens or closes the menu on Escape and, while it is open, handles its keys and clicks.
     * @param frame The frame's input and camera.
     * @param alone Whether the local player is the only one in the world.
     */
    void HandleInput(const svanes::FrameContext& frame, bool alone);

    /**
     * Places the menu and speed label over the current view. Call after the camera has moved.
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

    /** @return The applied speed. */
    GooseSpeed Speed() const;

    /** Returns the game to 1x, as when another player joins. */
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
     * Moves the menu's areas so the menu stays centered in the viewport.
     * @param viewport The viewport in screen coordinates.
     */
    void Layout(const svanes::Rectangle2D& viewport);

    bool open = false;
    bool quit_requested = false;
    GooseSpeed speed = GooseSpeed::Normal;
    GooseSpeed highlighted = GooseSpeed::Normal;

    svanes::Rectangle2D backdrop_area{};
    svanes::Entity backdrop{};
    svanes::Entity highlight{};
    std::array<Button, 3> speed_buttons{};
    Button quit_button{};
    svanes::Entity title{};
    svanes::Entity caption{};
    svanes::Entity note{};
    svanes::Entity hint{};
    svanes::Entity speed_label{};
};

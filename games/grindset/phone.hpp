#pragma once

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/registry.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <vector>

namespace grindset {

/**
 * The phone held up on the right side of the view. It is a dark body with
 * rounded corners, a screen inset inside the bezel, and a small camera hole at
 * the top of the screen, with a status bar across the top of the screen showing
 * the clock, always 5:30am, and the player's balance. It is drawn above the
 * road and the car interior, and is large, since it is what the player
 * interacts with.
 *
 * The phone is either held up or put away in the pocket, fully below the
 * bottom of the view, and eases between the two when toggled. It starts in the
 * pocket. While it is all the way in the pocket, a hint near where it comes up
 * tells the player how to take it out. While held, it sways gently as if in an
 * unsteady hand, without tilting, since text on it cannot rotate. The sway
 * fades in and out with the slide.
 *
 * Every part of the phone shares one Transform at the phone's center, with its
 * geometry placed relative to that center, so the whole phone moves by moving
 * that one position.
 */
class Phone final {
public:
    /**
     * Creates the body, the screen, the camera hole, and the status bar in the
     * pocket, and the hint for taking the phone out.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     * @param hint_font The font to write the hint in.
     * @param status_font The smaller font to write the status bar in.
     */
    void Initialize(svanes::Registry &world, svanes::FontHandle hint_font,
                    svanes::FontHandle status_font);

    /**
     * Starts moving the phone to the pocket if it is held or headed there,
     * or up into the hand otherwise. A slide already underway turns around
     * from where it is.
     */
    void Toggle();

    /**
     * Moves the phone along its slide toward the pocket or the hand, shows
     * the hint only while the phone is all the way in the pocket, and shows
     * the balance in the status bar.
     * @param world The world holding it.
     * @param seconds The time since the previous update, in seconds.
     * @param balance_cents The player's balance, in cents. Must not be
     * negative.
     */
    void Update(svanes::Registry &world, float seconds,
                std::int64_t balance_cents);

    /**
     * Returns where the phone's screen is, for apps to lay themselves out in.
     * @return The screen's bounds, ignoring its rounded corners.
     */
    svanes::Rectangle2D ScreenRectangle() const;

private:
    /**
     * Puts the phone where its slide currently has it, eased so it starts and
     * stops gently, plus as much of the hand's sway as the slide allows. The
     * status bar moves along with it.
     * @param world The world holding it.
     */
    void Place(svanes::Registry &world);

    std::vector<svanes::Entity> parts;
    svanes::Entity hint{};
    svanes::Entity clock{};
    svanes::Entity balance{};
    svanes::Vector2D position{};
    float slide = 0.0F;
    float elapsed = 0.0F;
    bool held = false;
};

} // namespace grindset

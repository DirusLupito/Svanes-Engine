#pragma once

#include <svanes/MenuUtilities/font_manager.hpp>
#include <svanes/entity.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/registry.hpp>
#include <svanes/vector2d.hpp>

#include <vector>

namespace grindset {

/**
 * The phone held up on the right side of the view. It is a dark body with
 * rounded corners, a screen inset inside the bezel, and a small camera hole at
 * the top of the screen. It is drawn above the road and the car interior, and
 * is large, since it is what the player interacts with.
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
     * Creates the body, the screen, and the camera hole in the pocket, and
     * the hint for taking the phone out.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     * @param font The font to write the hint in.
     */
    void Initialize(svanes::Registry &world, svanes::FontHandle font);

    /**
     * Starts moving the phone to the pocket if it is held or headed there,
     * or up into the hand otherwise. A slide already underway turns around
     * from where it is.
     */
    void Toggle();

    /**
     * Moves the phone along its slide toward the pocket or the hand, and
     * shows the hint only while the phone is all the way in the pocket.
     * @param world The world holding it.
     * @param seconds The time since the previous update, in seconds.
     */
    void Update(svanes::Registry &world, float seconds);

    /**
     * Returns where the phone's screen is, for apps to lay themselves out in.
     * @return The screen's bounds, ignoring its rounded corners.
     */
    svanes::Rectangle2D ScreenRectangle() const;

private:
    /**
     * Puts the phone where its slide currently has it, eased so it starts and
     * stops gently, plus as much of the hand's sway as the slide allows.
     * @param world The world holding it.
     */
    void Place(svanes::Registry &world);

    std::vector<svanes::Entity> parts;
    svanes::Entity hint{};
    svanes::Vector2D position{};
    float slide = 0.0F;
    float elapsed = 0.0F;
    bool held = false;
};

} // namespace grindset

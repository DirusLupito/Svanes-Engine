#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/rectangle_geometry.hpp>
#include <svanes/registry.hpp>
#include <svanes/vector2d.hpp>

#include <vector>

namespace grindset {

/**
 * The phone held up on the right side of the view. It is a dark body with
 * rounded corners, a screen inset inside the bezel, and a small camera hole at
 * the top of the screen. It is drawn above the road and nearly fills the
 * height of the view, since it is what the player interacts with.
 *
 * Every part of the phone shares one Transform at the phone's center, with its
 * geometry placed relative to that center, so the whole phone moves by moving
 * that one position.
 */
class Phone final {
public:
    /**
     * Creates the body, the screen, and the camera hole in the held position.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     */
    void Initialize(svanes::Registry &world);

    /**
     * Returns where the phone's screen is, for apps to lay themselves out in.
     * @return The screen's bounds, ignoring its rounded corners.
     */
    svanes::Rectangle2D ScreenRectangle() const;

private:
    std::vector<svanes::Entity> parts;
    svanes::Vector2D position{};
};

} // namespace grindset

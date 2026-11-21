#pragma once

#include <svanes/entity.hpp>
#include <svanes/registry.hpp>

#include <vector>

namespace grindset {

/**
 * The view out the windshield: a sunrise sky with the sun just above the
 * horizon, and a two lane road running from the bottom of the view to a
 * vanishing point at the center of the horizon. The camera sits in the middle
 * of the right lane, so the road's center line runs off to the left. The sky is
 * a stack of thin bands fading from deep blue at the top to pale orange at the
 * horizon, since the engine has no linear gradients. The ground below the
 * horizon hides the bottom of the sun.
 *
 * Center line dashes scroll toward the camera in perspective, growing as they
 * approach, and start over at the horizon once they pass the bottom of the
 * view.
 */
class Road final {
public:
    /**
     * Creates the sky bands, the sun and its glow, the ground, and the road
     * with its edge lines and center line dashes.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     */
    void Initialize(svanes::Registry &world);

    /**
     * Moves the center line dashes toward the camera.
     * @param world The world holding the dashes.
     * @param seconds The time since the previous update, in seconds.
     */
    void Update(svanes::Registry &world, float seconds);

private:
    std::vector<svanes::Entity> dashes;
    float dash_offset = 0.0F;
};

} // namespace grindset

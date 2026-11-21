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
 * Center line dashes, telephone poles on the left, and street lamps on the
 * right scroll toward the camera in perspective, growing as they approach, and
 * start over at the horizon once they pass the camera. The lamps reach out over
 * the road and are unlit, since the sun is up. Wires sag between neighboring
 * telephone poles. Poles and lamps fade in out of
 * the distance so they don't pop into view.
 */
class Road final {
public:
    /**
     * Creates the sky bands, the sun and its glow, the ground, and the road
     * with its edge lines, center line dashes, telephone poles and their
     * wires, and street lamps.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     */
    void Initialize(svanes::Registry &world);

    /**
     * Moves the dashes, poles, wires, and lamps toward the camera.
     * @param world The world holding them.
     * @param seconds The time since the previous update, in seconds.
     */
    void Update(svanes::Registry &world, float seconds);

private:
    /**
     * An entity of the road view and where it sits when the view is not
     * shifted.
     *
     * FIELDS:
     * - entity: The entity.
     * - resting_y: The y position of its Transform with no shift applied.
     */
    struct RestingShape {
        svanes::Entity entity;
        float resting_y;
    };

    /**
     * Moves every entity of the road view up or down from where it rests.
     * @param world The world holding them.
     * @param offset How far to move them, in pixels, positive downward.
     */
    void ShiftVertically(svanes::Registry &world, float offset);

    std::vector<RestingShape> shapes;
    std::vector<svanes::Entity> dashes;
    std::vector<svanes::Entity> telephone_poles;
    std::vector<svanes::Entity> street_lamps;
    std::vector<svanes::Entity> wires;
    float distance = 0.0F;
};

} // namespace grindset

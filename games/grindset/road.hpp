#pragma once

#include <svanes/entity.hpp>
#include <svanes/registry.hpp>

#include <array>
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
 *
 * Sideways positions on the road are measured from the middle of the right
 * lane, and everything is seen from the camera's sideways position, so the
 * whole view is projected again every update. The camera starts in the right
 * lane and can change to the left lane and back, easing sideways so it starts
 * and stops gently.
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
     * Moves the dashes, poles, wires, and lamps toward the camera, and lays
     * out the whole road as seen from where the camera is.
     * @param world The world holding them.
     * @param seconds The time since the previous update, in seconds.
     */
    void Update(svanes::Registry &world, float seconds);

    /**
     * Starts moving the camera to the left lane. A change already underway
     * turns around from where it is.
     */
    void SteerLeft();

    /**
     * Starts moving the camera to the right lane. A change already underway
     * turns around from where it is.
     */
    void SteerRight();

    /**
     * Returns how hard the car is steering, which is strongest halfway through
     * a lane change.
     * @return From -1 for steering hardest to the left to 1 for steering
     * hardest to the right, and 0 while staying in a lane.
     */
    float Steering() const;

    /**
     * Returns how far the road view is currently bobbed up or down.
     * @return The vertical shift, in pixels, positive downward.
     */
    float CurrentBob() const;

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
    std::array<svanes::Entity, 3> road_strips{};
    std::vector<svanes::Entity> dashes;
    std::vector<svanes::Entity> telephone_poles;
    std::vector<svanes::Entity> street_lamps;
    std::vector<svanes::Entity> wires;
    float distance = 0.0F;
    float elapsed = 0.0F;
    float camera_lateral = 0.0F;
    float lane = 0.0F;
    bool heading_left = false;
};

} // namespace grindset

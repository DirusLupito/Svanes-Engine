#pragma once

#include <svanes/entity.hpp>
#include <svanes/registry.hpp>

#include <vector>

namespace grindset {

/**
 * The inside of the car around the road view, seen from the driver's seat: a
 * roof edge across the top and the left pillar framing the windshield (the
 * right one is out of view), a dashboard across the bottom with the edge of
 * the hood just above it, the top of the steering wheel centered below the
 * view, and a rearview mirror hanging from the roof, partly behind the phone.
 * Everything is a plain placeholder shape. It draws above the road and below
 * the phone.
 *
 * The interior bobs along with the road, but by less, so the outside moves a
 * little against the car.
 */
class CarInterior final {
public:
    /**
     * Creates the roof, pillars, hood, dashboard, steering wheel, and mirror.
     * @param world The world to create them in. World coordinates are expected
     * to match screen pixels on a 1920x1080 view.
     */
    void Initialize(svanes::Registry &world);

    /**
     * Moves the interior up or down along with the road's bob.
     * @param world The world holding it.
     * @param road_bob How far the road view is currently bobbed, in pixels,
     * positive downward.
     */
    void Update(svanes::Registry &world, float road_bob);

private:
    std::vector<svanes::Entity> parts;
};

} // namespace grindset

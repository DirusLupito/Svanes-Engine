#pragma once

#include <svanes/entity.hpp>
#include <svanes/registry.hpp>

#include <vector>

namespace grindset {

/**
 * The inside of the car around the road view, seen from the driver's seat: a
 * roof edge across the top and a pillar down each side framing the
 * windshield, a dashboard across the bottom with the edge of the hood just
 * above it, the top of the steering wheel in the lower left, and a rearview
 * mirror hanging from the roof. Everything is a plain placeholder shape. It
 * draws above the road and below the phone.
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

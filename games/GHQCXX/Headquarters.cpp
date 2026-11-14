#include "Headquarters.hpp"

#include "PieceVisuals.hpp"

#include <svanes/registry.hpp>

namespace ghqcxx {

// Drawn as one colored star using the shared composite of triangles.
// Its bounding circle has a diameter of one world unit. The star points down
// before the supplied rotation is applied.

void CreateHeadquarters(svanes::Registry &world, svanes::Color color,
                        const svanes::Transform &transform) {
    const auto star = world.CreateEntity();
    world.AddComponent<svanes::Transform>(star, transform);
    world.AddComponent<svanes::SolidShape>(star, CreateStarVisual(color));
}

} // namespace ghqcxx

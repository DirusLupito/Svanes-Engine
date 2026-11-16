#include "Headquarters.hpp"

#include "PieceVisuals.hpp"

#include <svanes/registry.hpp>

#include <numbers>

namespace ghqcxx {

// Drawn as one colored star using the shared composite of triangles.
// Its bounding circle has a diameter of one world unit. Rotated
// half a turn so HQ faces up before the supplied rotation is applied.

std::vector<svanes::Entity>
CreateHeadquarters(svanes::Registry &world, svanes::Color color,
                   const svanes::Transform &transform) {
    const auto star = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        star, svanes::ComposeTransforms(
                  transform, {.rotation = std::numbers::pi_v<float>}));
    world.AddComponent<svanes::SolidShape>(star, CreateStarVisual(color));
    return {star};
}

} // namespace ghqcxx

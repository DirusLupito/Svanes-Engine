#include "BasicInfantry.hpp"

#include "PieceVisuals.hpp"

#include <svanes/registry.hpp>

namespace ghqcxx {

// Drawn as one white triangle and one colored rectangle.
// The triangle is the same infantry head used by parachute infantry. The
// rectangle uses the supplied color and meets the head's bottom edge at
// y = -0.1. The piece is one world unit tall and faces up.

// The rectangle is 0.4 units wide and 0.6 units tall, extending to y = 0.5.
// Its width is half the width of the infantry head's bottom edge.

void CreateBasicInfantry(svanes::Registry &world, svanes::Color color,
                         const svanes::Transform &transform) {
    const auto head = world.CreateEntity();
    world.AddComponent<svanes::Transform>(head, transform);
    world.AddComponent<svanes::SolidShape>(head, CreateInfantryHeadVisual());

    const auto body = world.CreateEntity();
    world.AddComponent<svanes::Transform>(
        body, svanes::ComposeTransforms(transform, {.y = 0.2F}));
    world.AddComponent<svanes::SolidShape>(
        body, svanes::SolidShape{.color = color,
                                 .geometry = svanes::Rectangle2D{
                                     .width = 0.4F, .height = 0.6F}});
}

} // namespace ghqcxx

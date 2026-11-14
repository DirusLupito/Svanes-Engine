#include "ParachuteInfantry.hpp"

#include "PieceVisuals.hpp"

#include <svanes/registry.hpp>

#include <cstdint>
#include <utility>

namespace ghqcxx {

// Drawn as six triangles, one circle, and one rectangle.
// The large white triangle forms the infantry head, with a narrow triangle in
// the supplied color pointing down below it. The piece is one world unit tall
// and faces up.

// The parachute canopy is a black circle with its lower half covered by a
// white rectangle, leaving a semicircle. Its radius is one-twelfth of the
// length of the infantry head's bottom edge.

// Two black triangles meet below the canopy. Two smaller white triangles
// draw over them, leaving black outlines for the suspension lines and a
// shared center line.

void CreateParachuteInfantry(svanes::Registry &world, svanes::Color color,
                             const svanes::Transform &transform) {
    const auto add_part = [&](svanes::SolidShape visual,
                              svanes::Transform local,
                              std::int32_t z_order = 0) {
        const auto entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(
            entity, svanes::ComposeTransforms(transform, local));
        world.AddComponent<svanes::SolidShape>(entity, std::move(visual));
        world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    };

    auto head = CreateInfantryHeadVisual();
    const auto white = head.color;
    add_part(std::move(head), {});
    add_part({.color = color,
              .geometry = svanes::Triangle2D{{svanes::Vector2D{-0.095F, -0.06F},
                                              {0.095F, -0.06F},
                                              {0.0F, 0.5F}}}},
             {});

    // The infantry head's bottom edge is 0.8 units wide, so the canopy radius
    // is 0.8 / 12. Cover the circle's lower half with the head's own white.
    constexpr float radius = 0.8F / 12.0F;
    add_part({.color = {}, .geometry = svanes::Circle2D{.radius = radius}},
             {.y = -0.28F}, 1);
    add_part({.color = white,
              .geometry = svanes::Rectangle2D{.width = 0.14F, .height = 0.07F}},
             {.y = -0.245F}, 2);

    // Two black triangles meet under the canopy. Smaller white triangles
    // cover their interiors, leaving the two outer lines and the center line.
    for (const float side : {-1.0F, 1.0F}) {
        add_part({.color = {},
                  .geometry =
                      svanes::Triangle2D{{svanes::Vector2D{side * radius, 0.0F},
                                          {0.0F, 0.0F},
                                          {0.0F, 0.13F}}}},
                 {.y = -0.28F}, 3);
        add_part(
            {.color = white,
             .geometry =
                 svanes::Triangle2D{{svanes::Vector2D{side * 0.052F, 0.006F},
                                     {side * 0.004F, 0.006F},
                                     {side * 0.004F, 0.108F}}}},
            {.y = -0.28F}, 4);
    }
}

} // namespace ghqcxx

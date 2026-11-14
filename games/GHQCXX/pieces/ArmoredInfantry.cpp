#include "ArmoredInfantry.hpp"

#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <cstdint>
#include <utility>

namespace ghqcxx {

// Drawn as two triangles and two black circles.
// The larger triangle uses the supplied color. A smaller white triangle draws
// over its top, sharing the same tip and sloping edges. This leaves the lower
// portion colored, with the white section meeting it at y = -0.1. The piece is
// one world unit tall and faces up.

// The colored triangle is 0.8 units wide at its base. The white triangle is
// 0.4 units tall and 0.32 units wide at its base, keeping the same proportions.
// Two circles sit inside the white section, each with a diameter of 0.1 units.

void CreateArmoredInfantry(svanes::Registry &world, svanes::Color color,
                           const svanes::Transform &transform) {
    // Apply the same initial position and rotation to every part, keeping the
    // triangles and circles together when placing the piece.
    const auto add_part = [&](svanes::SolidShape visual,
                              svanes::Transform local,
                              std::int32_t z_order = 0) {
        const auto entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(
            entity, svanes::ComposeTransforms(transform, local));
        world.AddComponent<svanes::SolidShape>(entity, std::move(visual));
        world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
    };

    add_part({.color = color,
              .geometry = svanes::Triangle2D{{svanes::Vector2D{0.0F, -0.5F},
                                              {-0.4F, 0.5F},
                                              {0.4F, 0.5F}}}},
             {});
    add_part({.color = {248, 248, 248},
              .geometry = svanes::Triangle2D{{svanes::Vector2D{0.0F, -0.5F},
                                              {-0.16F, -0.1F},
                                              {0.16F, -0.1F}}}},
             {}, 1);

    for (const float y : {-0.31F, -0.18F}) {
        add_part({.color = {}, .geometry = svanes::Circle2D{.radius = 0.05F}},
                 {.y = y}, 2);
    }
}

} // namespace ghqcxx

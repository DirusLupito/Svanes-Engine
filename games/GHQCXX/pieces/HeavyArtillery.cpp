#include "HeavyArtillery.hpp"

#include "PieceVisuals.hpp"

#include <svanes/registry.hpp>

#include <cstdint>
#include <numbers>
#include <utility>

namespace ghqcxx {

// Drawn as a convex hexagon, one white rectangle, and three black stars.
// The hexagon uses the supplied color and forms the base, with a flat top and
// two sloping shoulders. The white rectangle covers the middle of its top
// edge, leaving a short flat ledge on either side. It ends above the base's
// bottom edge, leaving a continuous strip of color beneath it. The piece is
// one world unit tall and faces up.

// The white rectangle has the same width and top edge as basic artillery's,
// but is only 0.7 units tall. The three stars use the same size and spacing as
// basic artillery's two stars, starting at the same position near the top.

std::vector<svanes::Entity>
CreateHeavyArtillery(svanes::Registry &world, svanes::Color color,
                     const svanes::Transform &transform) {
    std::vector<svanes::Entity> parts;

    // Apply the same initial position and rotation to every part, keeping the
    // base, rectangle, and stars together when placing the piece.
    const auto add_part = [&](svanes::SolidShape visual,
                              svanes::Transform local,
                              std::int32_t z_order = 0) {
        const auto entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(
            entity, svanes::ComposeTransforms(transform, local));
        world.AddComponent<svanes::SolidShape>(entity, std::move(visual));
        world.AddComponent<svanes::ZOrder>(entity, svanes::ZOrder{z_order});
        parts.push_back(entity);
    };

    add_part({.color = color,
              .geometry = svanes::ConvexPolygon2D{{{-0.275F, 0.0F},
                                                   {0.275F, 0.0F},
                                                   {0.4F, 0.125F},
                                                   {0.4F, 0.5F},
                                                   {-0.4F, 0.5F},
                                                   {-0.4F, 0.125F}}}},
             {});

    // The rectangle runs from y = -0.5 to 0.2. Drawing it over the hexagon
    // leaves 0.3 units of the colored base visible below it.
    add_part({.color = {248, 248, 248},
              .geometry = svanes::Rectangle2D{.width = 0.3F, .height = 0.7F}},
             {.y = -0.15F}, 1);

    for (const float y : {-0.4F, -0.275F, -0.15F}) {
        add_part(CreateStarVisual({}, 0.1F),
                 {.y = y, .rotation = std::numbers::pi_v<float>}, 2);
    }
    return parts;
}

} // namespace ghqcxx

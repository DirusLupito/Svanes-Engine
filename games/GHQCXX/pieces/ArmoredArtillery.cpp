#include "ArmoredArtillery.hpp"

#include "PieceVisuals.hpp"

#include <svanes/registry.hpp>

#include <cstdint>
#include <numbers>
#include <utility>

namespace ghqcxx {

// Drawn as two triangles, one white rectangle, two black stars, and two black
// circles. The triangles use the supplied color and meet the sides of the
// rectangle without gaps. All three shapes share their bottom edge. The piece
// is one world unit tall and faces up.

// The white rectangle and the two stars use the same dimensions and positions
// as basic artillery's. Two circles sit near the bottom of the rectangle, each
// with a diameter of 0.1 units, matching the stars' bounding circles.

std::vector<svanes::Entity>
CreateArmoredArtillery(svanes::Registry &world, svanes::Color color,
                       const svanes::Transform &transform) {
    std::vector<svanes::Entity> parts;

    // Apply the same initial position and rotation to every part, keeping the
    // triangles, rectangle, stars, and circles together when placing the piece.
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

    for (const float side : {-1.0F, 1.0F}) {
        add_part({.color = color,
                  .geometry =
                      svanes::Triangle2D{{svanes::Vector2D{side * 0.15F, -0.1F},
                                          {side * 0.4F, 0.5F},
                                          {side * 0.15F, 0.5F}}}},
                 {});
    }
    add_part({.color = {248, 248, 248},
              .geometry = svanes::Rectangle2D{.width = 0.3F, .height = 1.0F}},
             {});

    for (const float y : {-0.4F, -0.275F}) {
        add_part(CreateStarVisual({}, 0.1F),
                 {.y = y, .rotation = std::numbers::pi_v<float>}, 1);
    }

    for (const float y : {0.25F, 0.4F}) {
        add_part({.color = {}, .geometry = svanes::Circle2D{.radius = 0.05F}},
                 {.y = y}, 1);
    }
    return parts;
}

} // namespace ghqcxx

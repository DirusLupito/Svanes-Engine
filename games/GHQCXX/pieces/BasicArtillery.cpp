#include "BasicArtillery.hpp"

#include "PieceVisuals.hpp"

#include <svanes/registry.hpp>

#include <cstdint>
#include <numbers>
#include <utility>

namespace ghqcxx {

// Drawn as three rectangles and two black stars.
// The middle rectangle is white, and the shorter side rectangles use the
// supplied color. The piece is one world unit tall and faces up.

// The upper star starts 5% of the middle rectangle's height below its top edge.
// The second star sits below it. Each star's bounding circle has a diameter of
// one-third of the middle rectangle's width, limiting the distance between any
// two vertices to that size.

std::vector<svanes::Entity>
CreateBasicArtillery(svanes::Registry &world, svanes::Color color,
                     const svanes::Transform &transform) {
    std::vector<svanes::Entity> parts;

    // Apply the same initial position and rotation to every part, keeping the
    // rectangles and stars together when placing the piece.
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

    add_part({.color = {248, 248, 248},
              .geometry = svanes::Rectangle2D{.width = 0.3F, .height = 1.0F}},
             {});

    // The side rectangles meet the middle rectangle's sides and share its
    // bottom edge.
    for (const float x : {-0.275F, 0.275F}) {
        add_part(
            {.color = color,
             .geometry = svanes::Rectangle2D{.width = 0.25F, .height = 0.6F}},
            {.x = x, .y = 0.2F});
    }

    // The middle rectangle starts at y = -0.5. The first star's top is -0.45,
    // leaving 5% of the rectangle's height above it. A diameter of 0.1 is
    // one-third of the rectangle's width. Both stars point up and draw on top.
    for (const float y : {-0.4F, -0.275F}) {
        add_part(CreateStarVisual({}, 0.1F),
                 {.y = y, .rotation = std::numbers::pi_v<float>}, 1);
    }
    return parts;
}

} // namespace ghqcxx

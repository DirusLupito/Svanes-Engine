#include "PieceVisuals.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace ghqcxx {

svanes::SolidShape CreateStarVisual(svanes::Color color) {
    // Walk around the outline, alternating between a point and an inner
    // corner. Positive y points down, so the first point is the bottom tip.
    constexpr std::array<svanes::Vector2D, 10> outline{{
        {0.0F, 0.5F},
        {-0.1123F, 0.1545F},
        {-0.4755F, 0.1545F},
        {-0.1816F, -0.0590F},
        {-0.2939F, -0.4045F},
        {0.0F, -0.1910F},
        {0.2939F, -0.4045F},
        {0.1816F, -0.0590F},
        {0.4755F, 0.1545F},
        {0.1123F, 0.1545F},
    }};

    svanes::CompositeShape2D star;
    star.parts.reserve(outline.size());

    // Join each edge to the center. The triangles fill the star without
    // filling the gaps between its points.
    for (std::size_t index = 0; index < outline.size(); ++index) {
        star.parts.push_back({
            .shape =
                svanes::Triangle2D{{svanes::Vector2D{}, outline[index],
                                    outline[(index + 1) % outline.size()]}},
            .transform = {},
        });
    }

    return {.color = color, .geometry = std::move(star)};
}

} // namespace ghqcxx

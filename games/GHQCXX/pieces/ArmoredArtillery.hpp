#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/basic_render_types.hpp>

#include <vector>

namespace svanes {
class Registry;
}

namespace ghqcxx {

/**
 * Adds armored artillery to the registry.
 *
 * @param world The registry that owns the created visual entities.
 * @param color The color of the two side triangles.
 * @param transform The initial position and rotation applied to all parts.
 * Its origin is the center of the middle rectangle, and its rotation is in
 * radians.
 *
 * @return The entities making up the piece.
 */
std::vector<svanes::Entity>
CreateArmoredArtillery(svanes::Registry &world, svanes::Color color,
                       const svanes::Transform &transform);

} // namespace ghqcxx

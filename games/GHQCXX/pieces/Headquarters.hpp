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
 * Adds headquarters to the registry.
 *
 * @param world The registry that owns the created visual entity.
 * @param color The color of the star.
 * @param transform The initial position and rotation applied to the star.
 * Its origin is the center of the star, and its rotation is in radians.
 *
 * @return The entities making up the piece.
 */
std::vector<svanes::Entity>
CreateHeadquarters(svanes::Registry &world, svanes::Color color,
                   const svanes::Transform &transform);

} // namespace ghqcxx

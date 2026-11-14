#pragma once

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/basic_render_types.hpp>

namespace svanes {
class Registry;
}

namespace ghqcxx {

/**
 * Adds basic infantry to the registry.
 *
 * @param world The registry that owns the created visual entities.
 * @param color The color of the rectangle below the white infantry head.
 * @param transform The initial position and rotation applied to both parts.
 * Its origin is the center of the piece's one-unit height, and its rotation is
 * in radians.
 */
void CreateBasicInfantry(svanes::Registry &world, svanes::Color color,
                         const svanes::Transform &transform);

} // namespace ghqcxx

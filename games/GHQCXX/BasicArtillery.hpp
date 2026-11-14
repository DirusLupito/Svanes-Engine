#pragma once

#include <svanes/geometry/primitive_geometry.hpp>
#include <svanes/render/basic_render_types.hpp>

namespace svanes {
class Registry;
}

namespace ghqcxx {

/**
 * Adds basic artillery to the registry.
 *
 * @param world The registry that owns the created visual entities.
 * @param color The color of the two side rectangles.
 * @param transform The initial position and rotation applied to all parts.
 * Its origin is the center of the middle rectangle, and its rotation is in
 * radians.
 */
void CreateBasicArtillery(svanes::Registry &world, svanes::Color color,
                          const svanes::Transform &transform);

} // namespace ghqcxx

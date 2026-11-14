#pragma once

#include <svanes/render/render_system.hpp>

namespace ghqcxx {

/**
 * Creates a five-pointed star from a composite of triangles. The star is
 * centered at (0, 0), has an outer radius of 0.5 world units, and points down.
 * An entity's Transform positions and rotates the entire star about its center.
 *
 * @param color The color used to fill the star.
 *
 * @return A solid shape containing the star's triangles.
 */
svanes::SolidShape CreateStarVisual(svanes::Color color);

} // namespace ghqcxx

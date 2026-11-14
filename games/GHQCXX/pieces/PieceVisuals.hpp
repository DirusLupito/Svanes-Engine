#pragma once

#include <svanes/render/render_system.hpp>

namespace ghqcxx {

/**
 * Creates a five-pointed star from a composite of triangles. The star is
 * centered at (0, 0) and points down. Its size is specified by the diameter of
 * the circle containing its outer points.
 * An entity's Transform positions and rotates the entire star about its center.
 *
 * @param color The color used to fill the star.
 * @param diameter The diameter of the star's bounding circle, in world units.
 * Defaults to 1.0.
 *
 * @return A solid shape containing the star's triangles.
 *
 * @throws std::invalid_argument if diameter is not finite or is not positive.
 */
svanes::SolidShape CreateStarVisual(svanes::Color color, float diameter = 1.0F);

/**
 * Creates the white triangle shared by infantry piece visuals.
 *
 * @return A white triangle, 0.8 world units wide and 0.4 tall, with its tip at
 * (0, -0.5) and its bottom edge at y = -0.1.
 */
svanes::SolidShape CreateInfantryHeadVisual();

} // namespace ghqcxx

#pragma once

#include <svanes/render/basic_render_types.hpp>

#include <optional>

namespace svanes {

struct Transform;

/**
 * Component for 2D entity collision detection.
 * 
 */
struct Collider2D {
};

/**
 * Calculates the amount of overlap between two rectangular entities, a and b.
 * Returns the rectangular area of the overlap, or null if there is no overlap.
 *
 * @param a The first entity being checked for overlap
 * @param b The second entity being checked for overlap
 * 
 * @return std::optional<Rectangle> The rectangular area of the overlap, or null if there is no overlap
 */
std::optional<Rectangle> GetOverlap(const Transform& a, const Transform& b);

} // namespace svanes

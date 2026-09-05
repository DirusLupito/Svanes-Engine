#pragma once

#include <svanes/render/basic_render_types.hpp>

#include <optional>

namespace svanes {

struct Transform;

/**
 * Component for 2D entity collision detection.
 * 
 * FIELDS:
 * - is_static: Flag indicating whether an entity is static (true) or movable (false)
 */
struct Collider2D {
    bool is_static = false;
};

/**
 * Calculates the amount of overlap between two rectangular entities, a and b.
 * Returns the rectangular area of the overlap, or null if there is no overlap.
 *
 * a - The first entity being checked for overlap
 * b - The second entity being checked for overlap
 */
std::optional<Rectangle> GetOverlap(const Transform& a, const Transform& b);

} // namespace svanes

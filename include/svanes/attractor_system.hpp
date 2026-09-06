#pragma once

#include <svanes/entity.hpp>
#include <svanes/vector2d.hpp>

#include <functional>
#include <optional>
#include <unordered_map>

namespace svanes {

class Registry;

/**
 * Represents a point attractor in a 2D space,
 * which can influence the acceleration of other entities based on their relative position to the attractor.
 * Alternatively, it can be a repulsor if the acceleration is in the opposite direction of the offset.
 * 
 * FIELDS:
 * 
 * =======
 * 
 * - accelerationField: The acceleration field function. Implemented by the user to define how the attractor
 *          influences other entities based on their relative position to the attractor.
 *          
 *          @param offset_to_source: The offset from the target entity to the attractor.
 *                                   Measured as (attractor_position - target_position).
 * 
 *          @return Vector2D: The acceleration vector to apply to the target entity.
 * 
 * =======
 * 
 * - cutoff_radius: An optional cutoff radius. If provided, the attractor will only influence entities
 *                  within this radius. If the distance from the attractor to the target entity exceeds
 *                  this radius, the attractor will have no effect on that entity.
 */
struct PointAttractor2D {
    std::function<Vector2D(Vector2D offset_to_source)> accelerationField;
    std::optional<float> cutoff_radius;
};

/**
 * Evaluates all PointAttractor2D components in the given world and computes the resulting accelerations 
 * for all entities that are influenced by these attractors.
 * 
 * @param world The registry containing all entities and their components.
 * 
 * @return A map of entities to their resulting acceleration vectors.
 */
std::unordered_map<Entity, Vector2D> EvaluateAttractors(const Registry& world);

}

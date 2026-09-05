#include <svanes/attractor_system.hpp>

#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>

#include <cmath>
#include <stdexcept>

namespace svanes {

std::unordered_map<Entity, Acceleration2D> EvaluateAttractors(const Registry& world)
{

    // Rather than take in an entity and iterate over all other entities to find attractors
    // and then calculate the total acceleration, we can instead iterate over all attractors
    // and apply their acceleration to all other entities. While right now this is about
    // as efficient as the other way, if we ever add a spatial partitioning system, this will
    // be far more efficient, as rather than have every entity query, say, all entities in
    // the surrounding 9 grid cells, we can instead have each attractor query the surrounding 9
    // grid cells and apply its acceleration to all entities in those cells.

    // So for numEntities = N, numAttractors = A, and numEntitiesPerCell = E, 
    // that would take us from having N * 9 * E queries to A * 9 * E queries,
    // and for N >> A, this is a significant improvement.

    // Of course, we could just implement both ways and have the engine check
    // if N or A is larger and choose the more efficient method.

    std::unordered_map<Entity, Acceleration2D> accelerations;

    world.ForEach<Transform, PointAttractor2D>(
        [&](Entity source_entity, const Transform& source, const PointAttractor2D& attractor) {
            if (!attractor.accelerationField) {
                throw std::invalid_argument("PointAttractor2D requires an accelerationField function.");
            }

            if (attractor.cutoff_radius &&
                (!std::isfinite(*attractor.cutoff_radius) || *attractor.cutoff_radius < 0.0F)) {
                throw std::invalid_argument("PointAttractor2D cutoff_radius must be finite and nonnegative, or nullopt.");
            }

            // Will hopefully some day be replaced with a spatial lookup.
            world.ForEach<Transform, Kinematic2D>(
                [&](Entity target_entity, const Transform& target, const Kinematic2D&) {

                    // Special case: An attractor does not affect itself.
                    // If it did, the distance would be zero, and any acceleration field
                    // utilizing the distance may return a non-finite acceleration.
                    if (source_entity == target_entity) {
                        return;
                    }

                    // Our chosen convention is that the offset is measured as (attractor_position - target_position).

                    const float offset_to_source_x = source.x - target.x;
                    const float offset_to_source_y = source.y - target.y;


                    // nullopt cutoff radius means the attractor affects all entities, regardless of distance.
                    if (attractor.cutoff_radius &&
                        std::hypot(offset_to_source_x, offset_to_source_y) >= *attractor.cutoff_radius) {
                        return;
                    }

                    const Acceleration2D acceleration = attractor.accelerationField(offset_to_source_x, offset_to_source_y);

                    // With user defined functions, error checking should be far more strict.
                    if (!std::isfinite(acceleration.x) || !std::isfinite(acceleration.y)) {
                        throw std::runtime_error("PointAttractor2D accelerationField returned non-finite acceleration.");
                    }

                    Acceleration2D& total = accelerations[target_entity];
                    total.x += acceleration.x;
                    total.y += acceleration.y;
                }
            );
        }
    );

    return accelerations;
}

}

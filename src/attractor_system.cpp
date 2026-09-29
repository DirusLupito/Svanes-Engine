#include <svanes/attractor_system.hpp>

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/deterministic_math.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/registry.hpp>

#include <cmath>
#include <stdexcept>
#include <tuple>

namespace svanes {

std::vector<Vector2D>
EvaluateAttractors(const Registry &world,
                   std::span<const PhysicsTimeStep> entity_steps,
                   AsyncParallelForDriver &driver, std::size_t batch_size) {
    if (batch_size == 0) {
        throw std::invalid_argument(
            "Attractor parfor batch size must be positive.");
    }

    // Tuple used to store the source entity, its position, and a pointer to its
    // PointAttractor2D component so the three can be passed around together
    // for the sake of evaluating the acceleration field.
    using AttractorSource =
        std::tuple<Entity, Vector2D, const PointAttractor2D *>;

    // Attraction fields may not necessarily be parallel safe. However, we still
    // want to be able to evaluate attraction fields in parallel when possible.
    // What can we do? Idea: Let's give the game programmer the option to mark
    // an attractor as parallel safe, and if they do, the engine will evaluate
    // it in parallel with all the other parallel safe attractors. If they
    // don't, the engine will still evaluate it, but it will do so serially on
    // the calling thread. This way, the game programmer can still speed up
    // their attractors without having to worry about making every single one of
    // them parallel safe.

    // List of attractors that allow parallel evaluation.
    std::vector<AttractorSource> parallel_attractors;

    // List of attractors that must be evaluated serially on the calling thread.
    std::vector<AttractorSource> serial_attractors;
    std::vector<Vector2D> accelerations(entity_steps.size());

    world.ForEach<Transform, PointAttractor2D>(
        [&](Entity source_entity, const Transform &source,
            const PointAttractor2D &attractor) {
            if (!attractor.accelerationField) {
                throw std::invalid_argument(
                    "PointAttractor2D requires an accelerationField function.");
            }

            if (attractor.cutoff_radius &&
                (!std::isfinite(*attractor.cutoff_radius) ||
                 *attractor.cutoff_radius < 0.0F)) {
                throw std::invalid_argument(
                    "PointAttractor2D cutoff_radius must be finite and "
                    "nonnegative, or nullopt.");
            }


            // Sort each attractor into the appropriate list based on whether it
            // allows parallel evaluation or not.

            auto &attractors = attractor.allow_parallel ? parallel_attractors
                                                        : serial_attractors;

            attractors.emplace_back(source_entity, Vector2D{source.x, source.y},
                                    &attractor);
        });

    /**
     * Helper lambda to evaluate a single attractor's acceleration field on a
     * single target entity.
     *
     * @param source The source attractor's entity, position, and
     * PointAttractor2D component.
     * @param target_entity The entity being affected by the attractor.
     * @param target The position of the target entity.
     *
     * @return Vector2D The acceleration vector applied to the target entity by
     * the attractor.
     */
    const auto evaluate = [](const AttractorSource &source,
                             Entity target_entity, Vector2D target) {
        const auto &[source_entity, position, attractor] = source;

        // Special case: An attractor does not affect itself.
        // If it did, the distance would be zero, and any acceleration
        // field utilizing the distance may return a non-finite
        // acceleration.
        if (source_entity == target_entity) {
            return Vector2D{};
        }

        // Our chosen convention is that the offset is measured as
        // (attractor_position - target_position).
        const Vector2D offset_to_source = position - target;

        // nullopt cutoff radius means the attractor affects all
        // entities, regardless of distance.
        if (attractor->cutoff_radius &&
            Length(offset_to_source.x, offset_to_source.y) >=
                *attractor->cutoff_radius) {
            return Vector2D{};
        }

        const Vector2D acceleration =
            attractor->accelerationField(offset_to_source);

        // With user defined functions, error checking should be far
        // more strict.
        if (!std::isfinite(acceleration.x) || !std::isfinite(acceleration.y)) {
            throw std::runtime_error(
                "PointAttractor2D accelerationField returned "
                "non-finite acceleration.");
        }

        return acceleration;
    };

    // We need only analyze those entities we already know are
    // participating in this physics step, rather than iterating over
    // all entities with a transform, kinematic, and timeline.

    // Our lambda passed to the parallel for driver to evaluate all parallel
    // attractors on a range of target entities.
    const auto evaluate_parallel = [&](std::size_t begin, std::size_t end,
                                       std::uint32_t) {
        for (std::size_t i = begin; i < end; ++i) {
            const PhysicsTimeStep &step = entity_steps[i];
            if (step.delta_tics == 0) {
                continue;
            }

            const Transform &target =
                world.GetComponent<Transform>(step.entity);
            for (const AttractorSource &attractor : parallel_attractors) {
                accelerations[i] +=
                    evaluate(attractor, step.entity, {target.x, target.y});
            }
        }
    };

    // Should we have fewer physics time steps than the batch size,
    // we can just evaluate the parallel attractors on the calling thread
    // and skip useless thread creation and scheduling.

    if (!parallel_attractors.empty()) {
        if (entity_steps.size() <= batch_size) {
            evaluate_parallel(0, entity_steps.size(), 0);
        } else {
            driver.ParallelFor(entity_steps.size(), batch_size,
                               evaluate_parallel);
        }
    }

    // For serial attractors, rather than take in an entity and iterate over all
    // attractors to calculate the total acceleration, we instead iterate over
    // all attractors and apply their acceleration to all other entities. While
    // right now this is about as efficient as the other way, if we ever add a
    // spatial partitioning system, this will be far more efficient, as rather
    // than have every entity query, say, all entities in the surrounding 9 grid
    // cells, we can instead have each attractor query the surrounding 9 grid
    // cells and apply its acceleration to all entities in those cells.

    // So for numEntities = N, numAttractors = A, and numEntitiesPerCell = E,
    // that would take us from having N * 9 * E queries to A * 9 * E queries,
    // and for N >> A, this is a significant improvement.

    // Of course, we could just implement both ways and have the engine check
    // if N or A is larger and choose the more efficient method.

    // Will hopefully some day be replaced with a spatial lookup.
    for (const AttractorSource &attractor : serial_attractors) {
        for (std::size_t i = 0; i < entity_steps.size(); ++i) {
            const PhysicsTimeStep &step = entity_steps[i];
            if (step.delta_tics == 0) {
                continue;
            }

            const Transform &target =
                world.GetComponent<Transform>(step.entity);
            accelerations[i] +=
                evaluate(attractor, step.entity, {target.x, target.y});
        }
    }

    return accelerations;
}

} // namespace svanes

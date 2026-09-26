#include <svanes/collision_pass.hpp>

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/registry.hpp>
#include <svanes/spatial/hgrid2d.hpp>

#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace svanes {

// Potentially inefficient, we reconstruct a new HGrid2D for every collision
// pass. Still, this is almost certainly faster than the naive O(n^2) submit
// everything to DetectCollisions approach.

std::vector<EntityCollision2D>
DetectEntityCollisions(const Registry &world,
                       const std::vector<Entity> &entities,
                       AsyncParallelForDriver &driver, std::size_t batch_size) {

    if (batch_size == 0) {
        throw std::invalid_argument("Collision batch size must be positive.");
    }
    // First, we set up our HGrid2D.

    std::unordered_set<Entity> unique_entities;
    unique_entities.reserve(entities.size());

    // List of entries used to build the HGrid2D.
    std::vector<HGridEntry2D> entries;
    entries.reserve(entities.size());

    // Iterate over the input entities, validate their geometry and entity IDs,
    // and populate the entries vector with HGridEntry2D objects for every
    // entity that has a valid bounding box.
    for (Entity entity : entities) {
        if (!unique_entities.insert(entity).second) {
            throw std::invalid_argument(
                "Entity collision input requires unique entity IDs.");
        }

        const auto &collider = world.GetComponent<Collider2D>(entity);
        const auto &transform = world.GetComponent<Transform>(entity);
        const auto bounds = ComputeBounds(collider.geometry, transform);

        // Skip entities that do not have a valid bounding box.
        if (bounds) {
            entries.push_back({entity, *bounds});
        }
    }

    // Using our processed entries, build the HGrid2D.

    HGrid2D grid;
    grid.Rebuild(entries);

    //
    // BROAD PHASE: Find all pairs of entities that could potentially collide.
    //

    const auto pairs = grid.BuildCollisionPairs(driver, batch_size);

    // Lists of collision results, one per worker. Together they contain every
    // unique pair of entities that collide.
    const std::uint32_t worker_count =
        pairs.size() <= batch_size ? 1 : driver.GetConcurrency();

    // Each worker will have its own vector of collision results to avoid
    // contention on a single vector. We will merge them all together at the
    // end.
    std::vector<std::vector<EntityCollision2D>> worker_results(worker_count);

    // The work function that will be executed by each worker. Each worker will
    // be given a range of pairs to process, and will append any collision
    // results to its own vector of results.
    const auto detect_pairs = [&](std::size_t begin, std::size_t end,
                                  std::uint32_t worker_index) {
        // Figure out which vector of pairs this worker should append to.
        auto &results = worker_results[worker_index];

        // For every work item in the range assigned to this worker...
        for (std::size_t index = begin; index < end; ++index) {
            const auto &[a, b] = pairs[index];

            //
            // NARROW PHASE: For every pair of entities that could potentially
            // collide, check if they actually do collide, and if so, determine
            // the nature of the collision.
            //

            auto collisions =
                DetectCollisions(world.GetComponent<Collider2D>(a).geometry,
                                 world.GetComponent<Transform>(a),
                                 world.GetComponent<Collider2D>(b).geometry,
                                 world.GetComponent<Transform>(b));

            if (!collisions.empty()) {
                results.push_back({a, b, std::move(collisions)});
            }
        }
    };

    // If the number of work items is less than or equal to the batch size, we
    // can just run the work function in the main thread without creating any
    // worker threads.

    if (pairs.size() <= batch_size) {
        detect_pairs(0, pairs.size(), 0);
    } else {
        // Otherwise, we make use of our driver.
        driver.ParallelFor(pairs.size(), batch_size, detect_pairs);
    }

    // If there is only one worker (i.e. we set up the driver with a concurrency
    // of 1), there's no need to merge results, we simply move ownership and
    // return.
    if (worker_results.size() == 1) {
        return std::move(worker_results.front());
    }


    // The number of collision results across all workers.
    std::size_t result_count = 0;
    for (const auto &results : worker_results) {
        result_count += results.size();
    }

    // Now that we know exactly how many collision results there are, we can
    // reserve space in the final vector and then fill it in with each worker's
    // results tacked onto the end of the previous one.
    std::vector<EntityCollision2D> results;
    results.reserve(result_count);
    for (auto &local_results : worker_results) {
        for (auto &result : local_results) {
            results.push_back(std::move(result));
        }
    }
    return results;
}

} // namespace svanes

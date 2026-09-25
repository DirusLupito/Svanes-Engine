#include <svanes/collision_pass.hpp>

#include <svanes/registry.hpp>
#include <svanes/spatial/hgrid2d.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace svanes {

// Potentially inefficient, we reconstruct a new HGrid2D for every collision
// pass. Still, this is almost certainly faster than the naive O(n^2) submit
// everything to DetectCollisions approach.

std::vector<EntityCollision2D>
DetectEntityCollisions(const Registry &world,
                       const std::vector<Entity> &entities) {
    // First, we set up our HGrid2D.

    // Maps each entity to its index in the input vector, so we can ensure that
    // we only check each pair of entities once, and so we can retrieve the
    // entity IDs for the results of the collision detection.
    std::unordered_map<Entity, std::size_t> input_indices;
    input_indices.reserve(entities.size());

    // List of entries used to build the HGrid2D.
    std::vector<HGridEntry2D> entries;
    entries.reserve(entities.size());

    // Iterate over the input entities, validate their geometry and indices, and
    // populate the entries vector with HGridEntry2D objects for every entity
    // that has a valid bounding box.
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const Entity entity = entities[i];
        if (!input_indices.emplace(entity, i).second) {
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

    // List of collision results to return. Will be populated with every unique
    // pair of entities that collide.
    std::vector<EntityCollision2D> results;

    // List of indices of entities to pass to the narrow phase collision
    // detection (DetectCollisions). Will be filled in once for every entity in
    // the grid, and will contain every other entity that the broad
    // phase (HGrid2D queries) has determined could potentially collide with the
    // current entity.
    std::vector<std::size_t> candidates;

    for (const HGridEntry2D &entry : entries) {

        //
        // BROAD PHASE: Find all entities that could potentially collide with
        // the current entity.
        //

        const Entity a = entry.entity;
        const std::size_t i = input_indices.at(a);
        candidates.clear();

        // For every entity that could potentially collide with the current
        // entity, its a candidate for narrow phase collision detection if its
        // index in the input vector is greater than the current entity's index.
        // This ensures that we only check each pair of entities once in the
        // narrow phase, and that we don't check an entity against itself.
        for (Entity entity : grid.Query(entry.bounds)) {
            const std::size_t j = input_indices.at(entity);
            if (j > i) {
                candidates.push_back(j);
            }
        }
        std::sort(candidates.begin(), candidates.end());

        //
        // NARROW PHASE: For every entity that could potentially collide with
        // the current entity, check if they actually do collide, and if so,
        // determine the nature of the collision.
        //

        for (std::size_t j : candidates) {
            const Entity b = entities[j];

            auto collisions =
                DetectCollisions(world.GetComponent<Collider2D>(a).geometry,
                                 world.GetComponent<Transform>(a),
                                 world.GetComponent<Collider2D>(b).geometry,
                                 world.GetComponent<Transform>(b));

            if (!collisions.empty()) {
                results.push_back({a, b, std::move(collisions)});
            }
        }
    }
    return results;
}

} // namespace svanes

#include <svanes/collision_pass.hpp>

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
                       const std::vector<Entity> &entities) {
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

    // List of collision results to return. Will be populated with every unique
    // pair of entities that collide.
    std::vector<EntityCollision2D> results;

    //
    // BROAD PHASE: Find all pairs of entities that could potentially collide.
    //

    for (const auto &[a, b] : grid.BuildCollisionPairs()) {

        //
        // NARROW PHASE: For every pair of entities that could potentially
        // collide, check if they actually do collide, and if so, determine the
        // nature of the collision.
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
    return results;
}

} // namespace svanes

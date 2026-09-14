#include <svanes/collision_pass.hpp>

#include <svanes/registry.hpp>

#include <cstddef>
#include <utility>

namespace svanes {

std::vector<EntityCollision2D>
DetectEntityCollisions(const Registry &world,
                       const std::vector<Entity> &entities) {
    std::vector<EntityCollision2D> results;

    // Simple, inefficient all-pairs collision detection.
    // For each pair of entities, check if they have Collider2D and Transform components,
    // and if so, detect collisions between their geometries using their transforms.
    
    for (std::size_t i = 0; i < entities.size(); ++i) {
        for (std::size_t j = i + 1; j < entities.size(); ++j) {
            const Entity a = entities[i];
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

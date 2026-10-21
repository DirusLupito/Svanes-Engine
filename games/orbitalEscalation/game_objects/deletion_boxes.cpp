#include "deletion_boxes.hpp"

#include "../combat/damage_system.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/registry.hpp>

#include <array>

// The positions and sizes of the four deletion boxes surrounding the planet.
static constexpr std::array<svanes::Rectangle2D, 4> boxes = {
    svanes::Rectangle2D{-200000.0F, 0.0F, 100000.0F, 500000.0F},
    svanes::Rectangle2D{200000.0F, 0.0F, 100000.0F, 500000.0F},
    svanes::Rectangle2D{0.0F, -200000.0F, 500000.0F, 100000.0F},
    svanes::Rectangle2D{0.0F, 200000.0F, 500000.0F, 100000.0F}};

std::vector<OwnedEntity> CreateDeletionBoxes(svanes::Registry &world) {
    // The owners of the invisible colliders defining the world's boundaries.
    std::vector<OwnedEntity> zones;
    for (const auto &box : boxes) {
        auto &entity = zones.emplace_back(world);

        world.AddComponent<svanes::Collider2D>(entity.Get(),
                                               svanes::Collider2D{box});
    }
    return zones;
}

std::vector<svanes::Vector2D>
ApplyDeletionBoxes(svanes::Registry &world,
                   std::span<const OwnedEntity> boxes) {
    // List of the positions of objects that were deleted by the deletion boxes.
    std::vector<svanes::Vector2D> impacts;

    world.ForEach<svanes::Transform, svanes::Collider2D>(
        [&](svanes::Entity entity, const svanes::Transform &pose,
            const svanes::Collider2D &collider) {
            if (IsDead(world, entity)) {
                return;
            }

            for (const auto &box : boxes) {
                const auto &zone =
                    world.GetComponent<svanes::Collider2D>(box.Get());
                if (!svanes::DetectCollisions(collider.geometry, pose,
                                              zone.geometry, {})
                         .empty()) {
                    world.AddComponent<Health>(entity, Health{0.0F});
                    impacts.push_back({pose.x, pose.y});
                    break;
                }
            }
        });
    return impacts;
}

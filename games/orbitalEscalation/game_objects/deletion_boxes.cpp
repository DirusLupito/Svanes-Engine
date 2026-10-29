#include "deletion_boxes.hpp"

#include "../combat/damage_system.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/events/collision_event.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/registry.hpp>

#include <array>
#include <memory>
#include <utility>

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

        world.AddComponent<svanes::Transform>(entity.Get());
        world.AddComponent<DeletionBox>(entity.Get());
        world.AddComponent<svanes::Collider2D>(entity.Get(),
                                               svanes::Collider2D{box});
    }
    return zones;
}

void DeletionBoxCollisionHandler::Register(
    svanes::Registry &world, svanes::EventManager &events,
    std::function<void(svanes::Vector2D)> on_delete) {
    events.Subscribe(
        svanes::CollisionEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<svanes::CollisionEvent>>(
            [this, &world, on_delete = std::move(on_delete)](
                const svanes::CollisionEvent &event) {
                HandleCollision(world, event, on_delete);
            }));
}

void DeletionBoxCollisionHandler::HandleCollision(
    svanes::Registry &world, const svanes::CollisionEvent &event,
    const std::function<void(svanes::Vector2D)> &on_delete) {
    const auto &pair = event.collision;
    const bool a_is_box = world.HasComponent<DeletionBox>(pair.a);
    const bool b_is_box = world.HasComponent<DeletionBox>(pair.b);

    if (a_is_box == b_is_box) {
        return;
    }

    const auto entity = a_is_box ? pair.b : pair.a;
    if (IsDead(world, entity)) {
        return;
    }
    const auto &pose = world.GetComponent<svanes::Transform>(entity);
    const svanes::Vector2D position{pose.x, pose.y};
    world.AddComponent<Health>(entity, Health{0.0F});
    if (on_delete) {
        on_delete(position);
    }
}

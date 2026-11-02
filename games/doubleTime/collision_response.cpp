#include "collision_response.hpp"

#include <svanes/collision_pass.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/registry.hpp>
#include <svanes/tilemaps/tilemap.hpp>

#include <algorithm>
#include <memory>

void DoubleTimeCollisionHandler::Register(svanes::Registry &world,
                                          svanes::EventManager &events) {
    events.Subscribe(
        svanes::CollisionEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<svanes::CollisionEvent>>(
            [this, &world](const svanes::CollisionEvent &event) {
                HandleCollision(world, event);
            }));
}

void DoubleTimeCollisionHandler::BeginStep() {
    grounded = false;
    support.reset();
    triggers.clear();
}

void DoubleTimeCollisionHandler::Resolve(svanes::Registry &world,
                                         svanes::EventManager &events,
                                         svanes::AsyncParallelForDriver &driver,
                                         svanes::Entity character,
                                         svanes::Entity tilemap) {
    this->character = character;
    detecting_triggers = false;
    std::vector<svanes::Entity> solids;
    world.ForEach<svanes::Transform, svanes::Collider2D, DoubleTimeSolid>(
        [&](svanes::Entity entity, const auto &, const auto &, const auto &) {
            if (entity != character) {
                solids.push_back(entity);
            }
        });
    std::sort(solids.begin(), solids.end());
    solids.insert(solids.begin(), character);
    for (const bool axis : {true, false}) {
        horizontal = axis;
        for (std::uint32_t iteration = 0; iteration < 4; ++iteration) {
            origin = world.GetComponent<svanes::Transform>(character);
            deepest.reset();
            svanes::DetectEntityCollisions(world, solids, events, driver, 16,
                                           svanes::CollisionOrder::InputOrder);
            auto contacts = svanes::DetectTileMapCollisions(
                world.GetComponent<svanes::Collider2D>(character).geometry,
                origin, world.GetComponent<svanes::TileMap>(tilemap),
                world.GetComponent<svanes::Transform>(tilemap));
            if (!contacts.empty()) {
                events.QueueEvent(std::make_unique<svanes::CollisionEvent>(
                    svanes::EntityCollision2D{character, tilemap,
                                              std::move(contacts)}));
            }
            events.DispatchEvents();
            if (!deepest) {
                break;
            }
        }
    }
    this->character.reset();
}

void DoubleTimeCollisionHandler::DetectTriggers(
    svanes::Registry &world, svanes::EventManager &events,
    svanes::AsyncParallelForDriver &driver, svanes::Entity probe) {
    character = probe;
    detecting_triggers = true;
    triggers.clear();
    std::vector<svanes::Entity> entities;
    world.ForEach<svanes::Transform, svanes::Collider2D, DoubleTimeTrigger>(
        [&](svanes::Entity entity, const auto &, const auto &, const auto &) {
            entities.push_back(entity);
        });
    std::sort(entities.begin(), entities.end());
    entities.insert(entities.begin(), probe);
    svanes::DetectEntityCollisions(world, entities, events, driver, 16,
                                   svanes::CollisionOrder::InputOrder);
    events.DispatchEvents();
    character.reset();
    detecting_triggers = false;
}

void DoubleTimeCollisionHandler::HandleCollision(
    svanes::Registry &world, const svanes::CollisionEvent &event) {
    const auto &pair = event.collision;
    if (!character || (pair.a != *character && pair.b != *character)) {
        return;
    }
    const auto other = pair.a == *character ? pair.b : pair.a;
    if (detecting_triggers) {
        if (world.HasComponent<DoubleTimeTrigger>(other) &&
            std::find(triggers.begin(), triggers.end(), other) ==
                triggers.end()) {
            triggers.push_back(other);
        }
        return;
    }
    for (auto contact : pair.collisions) {
        if (pair.b == *character) {
            contact.normal.x = -contact.normal.x;
            contact.normal.y = -contact.normal.y;
        }
        const bool aligned =
            horizontal ? contact.normal.y == 0.0F : contact.normal.x == 0.0F;
        if (!aligned || contact.penetration_depth <= 0.0F ||
            (deepest &&
             contact.penetration_depth <= deepest->penetration_depth)) {
            continue;
        }
        deepest = contact;
        auto &pose = world.GetComponent<svanes::Transform>(*character);
        auto &motion = world.GetComponent<svanes::Kinematic2D>(*character);
        if (horizontal) {
            pose.x = origin.x + contact.normal.x * contact.penetration_depth;
            motion.velocity_x = 0.0F;
        } else {
            pose.y = origin.y + contact.normal.y * contact.penetration_depth;
            motion.velocity_y = 0.0F;
            if (contact.normal.y < 0.0F) {
                grounded = true;
                support = other;
            }
        }
    }
}

bool DoubleTimeCollisionHandler::IsGrounded() const { return grounded; }

std::optional<svanes::Entity> DoubleTimeCollisionHandler::GetSupport() const {
    return support;
}

const std::vector<svanes::Entity> &
DoubleTimeCollisionHandler::GetTriggers() const {
    return triggers;
}

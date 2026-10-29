#include "missile_system.hpp"

#include "../attachment_system.hpp"
#include "damage_system.hpp"
#include <svanes/events/collision_event.hpp>
#include <svanes/events/event_handler_wrapper.hpp>
#include <svanes/events/event_manager.hpp>
#include <svanes/registry.hpp>

#include <memory>
#include <unordered_set>

bool IgnoresFiringShip(const svanes::Registry &world, svanes::Entity missile,
                       svanes::Entity other) {
    if (!world.HasComponent<MissileExplosion>(missile)) {
        return false;
    }

    // The firing ship and any child attachment thereof are ignored so that
    // missiles don't explode on launch.
    const auto &explosion = world.GetComponent<MissileExplosion>(missile);
    if (!explosion.firing_ship ||
        GetAttachmentRoot(world, other) != *explosion.firing_ship) {
        return false;
    }

    const auto now =
        world.GetComponent<svanes::Timeline>(missile).GetTotalTics();

    return now - explosion.launched_at < explosion.arming_tics;
}

void MissileCollisionHandler::Register(svanes::Registry &world,
                                       svanes::EventManager &events) {
    events.Subscribe(
        svanes::CollisionEvent::GetStaticEventType(),
        std::make_unique<svanes::EventHandlerWrapper<svanes::CollisionEvent>>(
            [this, &world](const svanes::CollisionEvent &event) {
                HandleCollision(world, event);
            }));
}

void MissileCollisionHandler::HandleCollision(
    svanes::Registry &world, const svanes::CollisionEvent &event) {
    const auto &pair = event.collision;

    // Parts of the same object should not collide with each other. An arming
    // missile also ignores its firing ship and that ship's attached parts.

    if (GetAttachmentRoot(world, pair.a) == GetAttachmentRoot(world, pair.b) ||
        IgnoresFiringShip(world, pair.a, pair.b) ||
        IgnoresFiringShip(world, pair.b, pair.a)) {
        return;
    }

    for (const auto entity : {pair.a, pair.b}) {
        if (world.HasComponent<MissileExplosion>(entity) &&
            !world.GetComponent<MissileExplosion>(entity).detonated) {
            contacted_missiles.push_back(entity);
        }
    }
}

std::vector<svanes::Vector2D>
MissileCollisionHandler::UpdateExplosions(svanes::Registry &world) {
    // The positions of explosions produced by this update.
    std::vector<svanes::Vector2D> explosions;
    std::unordered_set<svanes::Entity> pending;

    // Finds undetonated missiles whose health has reached zero and adds them to
    // pending. It is called before processing begins and after each explosion
    // so missiles killed by blast damage are queued for chain reactions.
    const auto collect_dead = [&]() {
        world.ForEach<MissileExplosion, Health>(
            [&](svanes::Entity entity, const MissileExplosion &explosion,
                const Health &health) {
                if (!explosion.detonated && health.remaining <= 0.0F) {
                    pending.insert(entity);
                }
            });
    };
    collect_dead();

    // Add contacted missiles to the same queue as missiles found dead by
    // collect_dead.
    for (const auto missile : contacted_missiles) {
        pending.insert(missile);
    }
    contacted_missiles.clear();

    // Process the queue until all contact-triggered and chain-reaction
    // explosions have been handled.
    while (!pending.empty()) {
        const auto entity = *pending.begin();
        pending.erase(pending.begin());
        auto &explosion = world.GetComponent<MissileExplosion>(entity);

        // A missile may have been added more than once through contacts or
        // blast damage, but it must only explode once.
        if (explosion.detonated) {
            continue;
        }

        // Mark the missile before applying damage so a chain reaction cannot
        // queue this same missile indefinitely.
        explosion.detonated = true;
        const float damage = explosion.damage;
        const float radius = explosion.blast_radius;
        world.GetComponent<Health>(entity).remaining = 0.0F;
        const auto pose = world.GetComponent<svanes::Transform>(entity);
        ApplyAreaDamage(world, {pose.x, pose.y}, radius, damage);
        explosions.push_back({pose.x, pose.y});

        // The blast may have killed additional missiles, so add them before
        // selecting the next pending explosion.
        collect_dead();
    }
    return explosions;
}

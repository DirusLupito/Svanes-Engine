#include "missile_system.hpp"

#include "../attachment_system.hpp"
#include "../effects/collision_flashes.hpp"
#include "damage_system.hpp"
#include <svanes/registry.hpp>
#include <svanes/stable_id.hpp>

#include <set>

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

void UpdateMissileExplosions(
    svanes::Registry &world, svanes::Entity gameplay_timeline,
    std::span<const svanes::EntityCollision2D> contacts,
    std::vector<OwnedEntity> &flashes) {
    // One explosion can trigger another, so every peer needs to process them
    // in the same order. Local entity IDs do not provide that guarantee.
    const auto before = [&](svanes::Entity a, svanes::Entity b) {
        return world.GetComponent<svanes::StableId>(a).value <
               world.GetComponent<svanes::StableId>(b).value;
    };
    std::set<svanes::Entity, decltype(before)> pending(before);

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

    // Add contacted, undetonated missiles to the same queue as missiles found
    // dead by collect_dead.
    const auto contact = [&](svanes::Entity missile) {
        if (world.HasComponent<MissileExplosion>(missile) &&
            !world.GetComponent<MissileExplosion>(missile).detonated) {
            pending.insert(missile);
        }
    };
    for (const auto &pair : contacts) {
        contact(pair.a);
        contact(pair.b);
    }

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
        CreateCollisionFlash(world, gameplay_timeline, flashes,
                             {pose.x, pose.y});

        // The blast may have killed additional missiles, so add them before
        // selecting the next pending explosion.
        collect_dead();
    }
}

#pragma once

#include "../game_objects/owned_entity.hpp"
#include <optional>
#include <span>
#include <svanes/collision_pass.hpp>
#include <svanes/timeline_system.hpp>
#include <vector>

/**
 * Stores the explosion properties of a missile entity.
 *
 * FIELDS:
 * - damage: The damage dealt at the center of the explosion.
 * - blast_radius: The distance at which damage falls to zero.
 * - arming_tics: How long after launch to ignore contacts with the firing ship.
 * This uses the missile's local time. Contacts with other objects still count.
 * - firing_ship: The entity that fired this missile, if it has been launched.
 * Its still-attached parts are also ignored during the arming time.
 * - launched_at: The missile's total local tics when it was launched. Time
 * spent sitting in the launcher should not count toward arming.
 * - detonated: Whether this missile has already exploded. This prevents
 * simultaneous contacts, or another blast, from making the same missile explode
 * repeatedly.
 */
struct MissileExplosion {
    float damage;
    float blast_radius;
    svanes::TicCount arming_tics = 0;
    std::optional<svanes::Entity> firing_ship;
    svanes::TicCount launched_at = 0;
    bool detonated = false;
};

/**
 * Checks whether a contact is with the firing ship before the missile has
 * armed.
 *
 * @param world The registry containing the missile's timeline and attachments.
 * @param missile The entity that might be a recently launched missile.
 * @param other The other entity involved in the contact.
 *
 * @return Whether this contact should be ignored.
 */
bool IgnoresFiringShip(const svanes::Registry &world, svanes::Entity missile,
                       svanes::Entity other);

/**
 * Explodes missiles in the supplied contacts, or missiles that run out of
 * health even when there are no contacts. The caller has already removed
 * contacts between attached parts and contacts ignored during arming.
 * Every explosion causes area damage, even if the contacted entity has no
 * health. Only entities with health and collision geometry receive damage.
 * Damage can kill other missiles, so their explosions are processed here too.
 *
 * This marks missiles as dead but does not destroy registry entities. Their
 * owners remove them after all explosions have finished using their geometry.
 *
 * @param world The registry containing missiles and collision targets.
 * @param gameplay_timeline The parent timeline for the explosion flashes.
 * @param contacts The filtered collisions from this physics step.
 * @param flashes The owners of the active explosion flashes.
 */
void UpdateMissileExplosions(
    svanes::Registry &world, svanes::Entity gameplay_timeline,
    std::span<const svanes::EntityCollision2D> contacts,
    std::vector<OwnedEntity> &flashes);

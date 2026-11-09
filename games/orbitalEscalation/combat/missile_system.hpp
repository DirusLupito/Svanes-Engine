#pragma once

#include <optional>
#include <svanes/entity.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>
#include <vector>

namespace svanes {
class CollisionEvent;
class EventManager;
class Registry;
} // namespace svanes

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
 * Handles missile impacts by recording eligible collisions and resolving their
 * explosions after collision delivery. Contacts within an attachment tree and
 * contacts ignored during arming do not trigger explosions.
 */
class MissileCollisionHandler final {
public:
    /**
     * Registers the missile collision handler to receive collision events.
     *
     * @param world The registry containing missiles and collision targets.
     * @param events The event manager receiving collisions for this world.
     *
     * @throws std::invalid_argument if this handler type is already registered.
     * @throws std::logic_error if called during an event callback.
     */
    void Register(svanes::Registry &world, svanes::EventManager &events);

    /**
     * Explodes missiles recorded by the collision handler, or missiles that
     * run out of health even when there are no contacts. Call after collision
     * dispatch each physics step so physical response and planet damage finish
     * before blast damage is applied.
     *
     * Every explosion causes area damage, even if the contacted entity has no
     * health. Damage can kill other missiles, causing chain reactions.
     * Missiles are marked as dead but remain in the registry until their
     * owners remove them after all explosions have finished.
     *
     * @param world The registry containing missiles and collision targets.
     *
     * @return The world positions of the missiles which exploded.
     */
    std::vector<svanes::Vector2D> UpdateExplosions(svanes::Registry &world);

private:
    /**
     * Records missiles involved in an eligible collision so they can explode
     * after the other collision responses finish.
     *
     * @param world The registry containing the colliding entities.
     * @param event The collision to check for missile impacts.
     */
    void HandleCollision(svanes::Registry &world,
                         const svanes::CollisionEvent &event);

    // Missiles with eligible impacts awaiting explosion processing.
    std::vector<svanes::Entity> contacted_missiles;
};

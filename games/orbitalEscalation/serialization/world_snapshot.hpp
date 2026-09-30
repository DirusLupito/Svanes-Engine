#pragma once

#include "../combat/damage_system.hpp"
#include "../combat/missile_system.hpp"
#include "../combat/weapon_system.hpp"
#include "../propulsion_system.hpp"
#include <svanes/network/component_serialization.hpp>

/**
 * A saved clock that keeps gameplay timing consistent after restoration.
 *
 * FIELDS:
 * - value: The clock's scale, pause state, and exact progress.
 * - parent: The parent's shared identity, or zero for a root clock.
 */
struct SnapshotTimeline {
    svanes::Timeline value;
    std::uint64_t parent = 0;
};

/**
 * An attachment's placement relative to its owner.
 *
 * FIELDS:
 * - parent: The owning object's shared identity.
 * - transform: The attachment's pose in its parent's coordinates.
 */
struct SnapshotAttachment {
    std::uint64_t parent;
    svanes::Transform transform;
};

/**
 * Saves the changes to an object during gameplay so it can be restored from
 * its definition. Shared identities let peers resolve references to the same
 * objects.
 *
 * FIELDS:
 * - transform: The entity's world pose.
 * - motion: Independent movement and its limits, if present.
 * - timeline: The entity's clock and shared parent, if present.
 * - health: Remaining health, if present.
 * - propulsion_control: Propulsion requests held by the entity, if present.
 * - weapon_control: A pending firing request, if the component exists.
 * - explosion: Saved launch time and detonation state, if present.
 * - firing_ship: The firing ship's shared identity, or zero if absent.
 * - launcher: Saved reload progress and ammunition-loss state, if present.
 * - loaded_missile: Loaded ammunition's shared identity, or zero if absent.
 * - attachment: The owning object and relative pose, if attached.
 */
struct SnapshotBody {
    svanes::Transform transform;
    std::optional<svanes::Kinematic2D> motion;
    std::optional<SnapshotTimeline> timeline;
    std::optional<Health> health;
    std::optional<PropulsionControl> propulsion_control;
    std::optional<WeaponControl> weapon_control;
    std::optional<MissileExplosion> explosion;
    std::uint64_t firing_ship = 0;
    std::optional<MissileLauncherState> launcher;
    std::uint64_t loaded_missile = 0;
    std::optional<SnapshotAttachment> attachment;
};

/**
 * A dynamic object and the attachment ownership needed to rebuild it.
 *
 * FIELDS:
 * - identity: The object's unique, nonzero shared identity.
 * - type: The kind of dynamic object to construct.
 * - name: The catalogued definition within that type.
 * - body: The object's saved gameplay components.
 * - children: Shared identities of owned attachments, in attachment order.
 */
struct SnapshotObject {
    std::uint64_t identity;
    DynamicObjectType type;
    std::string name;
    SnapshotBody body;
    std::vector<std::uint64_t> children;
};

/**
 * A roster entry, which remains present even after the player's ship dies.
 *
 * FIELDS:
 * - peer: The player's unique, nonzero peer ID.
 * - ship: The owned ship's shared identity, or zero if it has been destroyed.
 * - touching_planet: Whether contact was already established on the previous
 * tick, so restoring contact does not cause another impact.
 */
struct SnapshotPlayer {
    std::uint32_t peer;
    std::uint64_t ship;
    bool touching_planet;
};

/**
 * Saves the gameplay state for rollback, synchronizing joining peers, and
 * checking that peers agree on the state of the world.
 *
 * FIELDS:
 * - next_identity: The next unused shared identity.
 * - root_identity: The gameplay timeline's shared identity.
 * - root: The gameplay timeline's saved clock state.
 * - planet_identity: The planet's shared identity.
 * - planet: The planet's saved gameplay components.
 * - players: Roster entries in ascending peer ID order.
 * - objects: Dynamic objects in ascending shared identity order.
 * - projectiles: Shared identities of independently owned projectiles, in
 * their simulation order.
 */
struct WorldSnapshot {
    std::uint64_t next_identity;
    std::uint64_t root_identity;
    SnapshotTimeline root;
    std::uint64_t planet_identity;
    SnapshotBody planet;
    std::vector<SnapshotPlayer> players;
    std::vector<SnapshotObject> objects;
    std::vector<std::uint64_t> projectiles;
};

/**
 * Encodes a snapshot for storing or sharing the saved gameplay state.
 *
 * @param snapshot The saved gameplay state to encode.
 * @return The message containing the snapshot.
 * @throws std::length_error if a collection or definition name exceeds the
 * message format's limits.
 */
svanes::NetworkMessage WriteWorldSnapshot(const WorldSnapshot &snapshot);

/**
 * Reads a saved snapshot so its gameplay state can be restored.
 *
 * @param bytes The complete message produced by WriteWorldSnapshot.
 * @return The saved gameplay state.
 * @throws std::invalid_argument if the message is truncated, a count, boolean,
 * clock or object type is invalid, or the message has trailing data.
 */
WorldSnapshot ReadWorldSnapshot(std::span<const std::byte> bytes);

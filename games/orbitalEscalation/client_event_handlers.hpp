#pragma once

#include "game_objects/planet.hpp"
#include "serialization/asset_catalog.hpp"
#include "serialization/world_update.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace svanes {
class Camera2D;
class EventManager;
class Registry;
} // namespace svanes

class PlayerDeathEvent;
class PlayerSpawnEvent;
class WorldSnapshotEvent;

/**
 * The client's world and resources used to apply server events.
 *
 * FIELDS:
 * - world: The registry containing the client's displayed objects.
 * - events: The event manager used to raise player spawn and death events.
 * - camera: The camera following this client's player ship.
 * - assets: The definitions used to recreate server objects.
 * - client_id: The server assigned ID of the local player.
 * - gameplay_timeline: The master timeline for displayed gameplay objects.
 * - effects_timeline: The local timeline for collision flashes.
 * - rotating_square: The square whose rotation comes from the server.
 * - pause_label: The label shown while the server is paused.
 * - planet: The locally owned planet, created on the first snapshot.
 * - player_ship: The local player's ship ID in the server's world, if alive.
 * - objects: The local objects indexed by their server entity IDs.
 * - root_objects: The owners of independent objects and their attachments.
 * - collision_flashes: The locally owned collision effects.
 */
struct ClientEventContext {
    svanes::Registry &world;
    svanes::EventManager &events;
    svanes::Camera2D &camera;
    const AssetCatalog &assets;
    const std::uint32_t &client_id;
    svanes::Entity gameplay_timeline;
    svanes::Entity effects_timeline;
    svanes::Entity rotating_square;
    svanes::Entity pause_label;
    std::optional<Planet> &planet;
    std::optional<svanes::Entity> &player_ship;
    std::unordered_map<svanes::Entity, DynamicObject *> &objects;
    std::vector<std::unique_ptr<DynamicObject>> &root_objects;
    std::vector<OwnedEntity> &collision_flashes;
};

/**
 * Applies server snapshots to the client's world.
 */
class ClientEventHandlers final {
public:
    /**
     * Prepares handlers for a client's world.
     *
     * @param context The world, resources, and object ownership used by the
     * handlers. Referenced objects must outlive the handlers.
     */
    explicit ClientEventHandlers(ClientEventContext context);

    /**
     * Reconciles the client's world with the server's complete snapshot.
     * Raises player death and spawn events when a player's ship changes,
     * then restores all objects and their attachment trees.
     * The player handlers must be registered with the context's event manager.
     *
     * @param event The server snapshot to apply.
     * @throws std::invalid_argument if a player ship or object definition is
     * invalid.
     * @throws std::logic_error if a required player handler is not registered.
     */
    void HandleWorldSnapshot(const WorldSnapshotEvent &event);

    /**
     * Creates a player's ship using its definition and state from the latest
     * snapshot. Attachments are supplied by the snapshot handler afterward.
     *
     * @param event The player whose ship is to be created.
     * @throws std::invalid_argument if the snapshot has no independent ship
     * for the player or the ship definition is invalid.
     * @throws std::logic_error if the player or ship already exists.
     */
    void HandlePlayerSpawn(const PlayerSpawnEvent &event);

    /**
     * Removes a player's ship and any objects still attached to it. Releases
     * local control if this is the client's player, and keeps the camera
     * in the last position.
     *
     * @param event The player whose ship is to be removed.
     * @throws std::out_of_range if the player has no known ship.
     */
    void HandlePlayerDeath(const PlayerDeathEvent &event);

    /**
     * Checks whether an entity is a player ship in the server's latest
     * snapshot.
     *
     * @param entity The entity in the client's registry.
     * @return true if the entity is a living player's ship in the latest
     * snapshot, false otherwise.
     */
    bool IsPlayerShip(svanes::Entity entity) const;

private:
    /**
     * Replaces an existing object's time, movement, health, and appearance
     * with the server's state.
     *
     * @param state The object's state, identified by its server entity ID.
     * @throws std::out_of_range if the object does not exist locally.
     */
    void ApplyObjectState(const WorldUpdate::Object &state);

    // The client's resources and object ownership updated by these handlers.
    ClientEventContext context;

    // The most recent complete world state received from the server.
    WorldUpdate snapshot{};

    // The living player ships, indexed by their players' server assigned IDs.
    std::unordered_map<std::uint32_t, svanes::Entity> player_ships;
};

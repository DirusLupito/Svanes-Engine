#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry.hpp>
#include <svanes/registry.hpp>

#include <functional>
#include <unordered_map>
#include <vector>

namespace svanes {

/**
 * Indicates wether an object is meant to be handled over the network.
 * Entities marked as Networked will be collected when the server executes
 * CollectTransformStates, entities not marked as such are excluded (haha losers)
 */
struct Networked {};

/**
 * Representation of an entities transform.
 * The server should have authority over the transform of all networked entities,
 * with clients reflecting the servers state.
 */
struct EntityTransformState {
    Entity network_entity;
    Transform transform;
};

/**
 * Pulls the transform states of networked entities from a world registry. As mentioned above,
 * CollectTransformStates filters out non-networked entities (because they are losers).
 * @param world The registry from which transform states are being pulled
 */
std::vector<EntityTransformState> CollectTransformStates(const Registry &world);

/**
 * Maps server entity IDs to local entity IDs.
 * Each client holds one to allow it to correctly interpret messages
 * from the server regarding entities in the game world.
 */
class NetworkEntityMap final {
public:
    /**
     * Resolves a network entity to a local one.
     * If there is no corresponding local entity, spawn a new one and store it in the map.
     * @param network_entity The network entity to be resolved
     * @param spawn_entity Callback for spawning a new entity if no local entity is found
     */
    Entity Resolve(Entity network_entity, const std::function<Entity()> &spawn_entity);

private:
    std::unordered_map<Entity, Entity> remote_to_local;
};

/**
 * Applies a transform state broadcast from the server to the actual local entity
 * @param world The world registry being updated
 * @param entity_map The map used to try to resolve a network entity to local
 * @param state The new transform state to be applied
 * @param spawn_entity Callback in case no local entity is found, passed in to Resolve()
 */
void ApplyTransformState(
    Registry &world, NetworkEntityMap &entity_map, const EntityTransformState &state,
    const std::function<Entity()> &spawn_entity
);

} // namespace svanes

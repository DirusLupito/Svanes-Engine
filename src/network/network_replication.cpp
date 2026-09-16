#include <svanes/network/network_replication.hpp>

namespace svanes {

std::vector<EntityTransformState> CollectTransformStates(const Registry &world) {
    std::vector<EntityTransformState> states;
    
    world.ForEach<Networked, Transform>([&](Entity entity, const Networked &, const Transform &transform) {
        states.push_back(EntityTransformState{entity, transform});
    });

    return states;
}

Entity NetworkEntityMap::Resolve(Entity network_entity, const std::function<Entity()> &spawn_entity) {
    const auto existing = remote_to_local.find(network_entity);
    if (existing != remote_to_local.end()) {
        return existing->second;
    }

    const Entity local_entity = spawn_entity();
    remote_to_local.emplace(network_entity, local_entity);
    return local_entity;
}

void ApplyTransformState(
    Registry &world, NetworkEntityMap &entity_map, const EntityTransformState &state,
    const std::function<Entity()> &spawn_entity
) {
    const Entity local_entity = entity_map.Resolve(state.network_entity, spawn_entity);
    world.AddComponent<Transform>(local_entity, state.transform);
}

} // namespace svanes

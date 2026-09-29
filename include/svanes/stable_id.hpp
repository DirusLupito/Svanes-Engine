#pragma once

#include <svanes/entity.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace svanes {

/**
 * An identity for an entity that every peer agrees on, unlike the local Entity
 * id, which differs between processes. The game assigns values, usually from a
 * counter that is part of its synchronized state, and never reuses one.
 *
 * FIELDS:
 * - value: The shared identity.
 */
struct StableId {
    std::uint64_t value = 0;
};

/**
 * Lists entities in ascending StableId order, which is the same on every peer,
 * unlike Registry::ForEach order.
 *
 * @tparam Components Further components each listed entity must have.
 * @param world The registry to search.
 * @return The matching entities, ordered by StableId.
 * @throws std::logic_error if two matching entities share a StableId.
 */
template <typename... Components>
std::vector<Entity> EntitiesByStableId(const Registry &world) {
    std::vector<Entity> entities;
    world.ForEach<StableId, Components...>(
        [&](Entity entity, const StableId &, const Components &...) {
            entities.push_back(entity);
        });
    std::sort(entities.begin(), entities.end(), [&](Entity a, Entity b) {
        return world.GetComponent<StableId>(a).value <
               world.GetComponent<StableId>(b).value;
    });
    const auto duplicate = std::adjacent_find(
        entities.begin(), entities.end(), [&](Entity a, Entity b) {
            return world.GetComponent<StableId>(a).value ==
                   world.GetComponent<StableId>(b).value;
        });
    if (duplicate != entities.end()) {
        throw std::logic_error(
            "Two entities share StableId " +
            std::to_string(world.GetComponent<StableId>(*duplicate).value));
    }
    return entities;
}

/**
 * @param world The registry to search.
 * @param value The StableId value to look for.
 * @return The entity with that StableId, or std::nullopt if none has it.
 */
std::optional<Entity> FindByStableId(const Registry &world,
                                     std::uint64_t value);

} // namespace svanes

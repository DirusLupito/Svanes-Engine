#include <svanes/registry.hpp>

namespace svanes {

Entity Registry::CreateEntity()
{
    if (!free_indices_.empty()) {
        const std::uint32_t index = free_indices_.back();
        free_indices_.pop_back();
        alive_[index] = true;
        return Entity{index, generations_[index]};
    }

    const std::uint32_t index = static_cast<std::uint32_t>(generations_.size());
    generations_.push_back(0);
    alive_.push_back(true);
    return Entity{index, 0};
}

void Registry::DestroyEntity(Entity entity)
{
    if (!IsValid(entity)) {
        return;
    }

    for (auto& entry : pools_) {
        entry.second->Remove(entity.index);
    }

    alive_[entity.index] = false;
    generations_[entity.index] += 1;
    free_indices_.push_back(entity.index);
}

bool Registry::IsValid(Entity entity) const
{
    return entity.index < generations_.size()
        && alive_[entity.index]
        && generations_[entity.index] == entity.generation;
}

}

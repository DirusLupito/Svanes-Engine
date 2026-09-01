#pragma once

#include <svanes/entity.hpp>

#include <any>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace svanes {

/**
* The entity component system registry. This class manages entities and their components.
* 
* It allows for creation and destruction of entities, as well as adding, removing, and 
* querying components associated with those entities.
* 
*/
class Registry final {
public:
    Entity CreateEntity();
    void DestroyEntity(Entity entity);

    /**
    * 
    */
    template <typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args)
    {
        std::any& slot = components[entity][std::type_index{typeid(T)}];
        slot = T(std::forward<Args>(args)...);
        return std::any_cast<T&>(slot);
    }

    template <typename T>
    void RemoveComponent(Entity entity)
    {
        auto entity_it = components.find(entity);
        if (entity_it != components.end()) {
            entity_it->second.erase(std::type_index{typeid(T)});
        }
    }

    template <typename T>
    bool HasComponent(Entity entity) const
    {
        auto entity_it = components.find(entity);
        if (entity_it == components.end()) {
            return false;
        }

        return entity_it->second.contains(std::type_index{typeid(T)});
    }

    template <typename T>
    T& GetComponent(Entity entity)
    {
        return std::any_cast<T&>(components.at(entity).at(std::type_index{typeid(T)}));
    }

    template <typename... Components, typename Func>
    void ForEach(Func&& func)
    {
        static_assert(sizeof...(Components) > 0, "ForEach requires at least one component type");

        for (auto& [entity, comps] : components) {
            if ((HasComponent<Components>(entity) && ...)) {
                func(entity, GetComponent<Components>(entity)...);
            }
        }
    }

private:
    Entity next_entity = 0;
    std::unordered_map<Entity, std::unordered_map<std::type_index, std::any>> components;
};

} // namespace svanes

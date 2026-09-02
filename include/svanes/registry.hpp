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
* Read through entity_components.md for a more detailed explanation of how components work with entities.
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
	* Adds a component of type T to the specified entity. If the entity already has a component of that type, it will be replaced.
    * 
	* Gets a reference to where the component will live in the registry, writes the new component to that location and passes in
	* the provided arguments to the component's constructor, and returns a reference to the newly added component
    * 
    * Example usage:
    * 
    * registry.AddComponent<svanes::Transform>(entity);
    * 
    * registry.AddComponent<svanes::SpriteAnimation>(entity, svanes::SpriteAnimation{
    *     .texture = orb_texture,
    *     .frame_width = 128,
    *     .frame_height = 128,
    *     .frame_count = 4,
    *     .seconds_per_frame = 0.12F,
    * });
    * 
    * @tparam T The component to add.
    * @param entity The entity to add the component to.
    * @param args The arguments to pass to the component's constructor.
    * @return A reference to the added component.
    */
    template <typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args)
    {
        std::any& slot = components[entity][std::type_index{typeid(T)}];
        slot = T(std::forward<Args>(args)...);
        return std::any_cast<T&>(slot);
    }

    /**
	* Removes a component from the given entity. 
    * 
    * If the entity does not have a component of that type, this function is a no-op.
    * 
    * @tparam T The component to remove.
	* @param entity The entity to remove the component from.
    */
    template <typename T>
    void RemoveComponent(Entity entity)
    {
        auto entity_it = components.find(entity);
        if (entity_it != components.end()) {
            entity_it->second.erase(std::type_index{typeid(T)});
        }
    }

    /**
	* Checks if the given entity has a component of the given type.
    * 
    * @tparam T The component to find.
    * @param entity The entity to check.
    * @return true if the entity has the component, false otherwise.
    */
    template <typename T>
    bool HasComponent(Entity entity) const
    {
        auto entity_it = components.find(entity);
        if (entity_it == components.end()) {
            return false;
        }

        return entity_it->second.contains(std::type_index{typeid(T)});
    }

    /**
	* Gets a reference to the component of the given type associated with the given entity.
    * 
    * @tparam T The component to get.
    * @param entity The entity to check.
    * @return A reference to the component.
    */
    template <typename T>
    T& GetComponent(Entity entity)
    {
        return std::any_cast<T&>(components.at(entity).at(std::type_index{typeid(T)}));
    }

    /**
	* Iterates over all entities that have all of the specified components.
    *
    * @tparam Components The component types to check for.
    * @tparam Func The type of the function to call for each entity.
    * @param func The function to call for each entity.
    */
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
	// The unique ID for each entity, which is incremented each time a new entity is created.
    Entity next_entity = 0;
    
    // A 2D map of all components in the registry, first indexed by entity and then component type.
    std::unordered_map<Entity, std::unordered_map<std::type_index, std::any>> components;
};

} // namespace svanes

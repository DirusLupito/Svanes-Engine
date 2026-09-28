#include "visuals.hpp"

#include <svanes/registry.hpp>

#include <utility>

Visuals::Visuals(svanes::Registry &world, std::vector<Visual> parts) {
    entities.reserve(parts.size());

    // For every visual part, create a new entity in the registry and add the
    // appropriate components to it. Each visual part is uniquely represented by
    // its own entity.
    for (Visual &part : parts) {
        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(entity);
        world.AddComponent<svanes::ZOrder>(entity,
                                           svanes::ZOrder{part.z_order});
        std::visit(
            [&]<typename Component>(Component &component) {
                world.AddComponent<Component>(entity, std::move(component));
            },
            part.component);
        entities.push_back(entity);
    }
}

void Visuals::SetTransform(svanes::Registry &world,
                           const svanes::Transform &transform) const {
    for (svanes::Entity entity : entities) {
        world.GetComponent<svanes::Transform>(entity) = transform;
    }
}

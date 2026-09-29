#include "planet.hpp"

#include <svanes/registry.hpp>

#include <utility>

Planet::Planet(svanes::Registry &world, PlanetDefinition definition)
    : entity(world.CreateEntity()),
      visuals(world, std::move(definition.visuals)) {
    world.AddComponent<svanes::Transform>(entity);
    world.AddComponent<svanes::Collider2D>(entity,
                                           std::move(definition.collider));
    world.AddComponent<svanes::PointAttractor2D>(
        entity, std::move(definition.attractor));
}

svanes::Entity Planet::GetEntity() const { return entity; }

svanes::Transform &Planet::GetTransform(svanes::Registry &world) const {
    return world.GetComponent<svanes::Transform>(entity);
}

void Planet::UpdateVisuals(svanes::Registry &world) const {
    visuals.SetTransform(world, GetTransform(world));
}

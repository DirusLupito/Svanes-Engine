#include "planet.hpp"

#include <svanes/registry.hpp>

#include <utility>

Planet::Planet(svanes::Registry &world, PlanetDefinition definition)
    : entity(world), visuals(world, std::move(definition.visuals)) {
    world.AddComponent<svanes::Transform>(entity.Get());
    world.AddComponent<svanes::Collider2D>(entity.Get(),
                                           std::move(definition.collider));
    world.AddComponent<svanes::PointAttractor2D>(
        entity.Get(), std::move(definition.attractor));
}

svanes::Entity Planet::GetEntity() const { return entity.Get(); }

svanes::Transform &Planet::GetTransform(svanes::Registry &world) const {
    return world.GetComponent<svanes::Transform>(entity.Get());
}

void Planet::UpdateVisuals(svanes::Registry &world) const {
    visuals.SetTransform(world, GetTransform(world));
}

#include "dynamic_object.hpp"

#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <utility>

/**
 * Helper function to create an entity with a Timeline component that is a
 * child of the provided gameplay_timeline entity. This is useful for creating
 * entities that should be synchronized with the main gameplay timeline.
 *
 * @param world The registry in which to create the entity.
 * @param gameplay_timeline The parent timeline entity to which the new entity's
 * Timeline will be linked.
 *
 * @return The newly created entity with a Timeline component.
 */
static svanes::Entity CreateTimedEntity(svanes::Registry &world,
                                        svanes::Entity gameplay_timeline) {
    const svanes::Entity entity = world.CreateEntity();
    world.AddComponent<svanes::Timeline>(entity, gameplay_timeline);
    return entity;
}

// pretty much pojo slop... for now...

DynamicObject::DynamicObject(svanes::Registry &world,
                             svanes::Entity gameplay_timeline,
                             svanes::Collider2D collider,
                             std::vector<Visual> visuals)
    : entity(CreateTimedEntity(world, gameplay_timeline)),
      visuals(world, std::move(visuals)) {
    world.AddComponent<svanes::Collider2D>(entity, std::move(collider));
    world.AddComponent<svanes::Kinematic2D>(entity);
    world.AddComponent<svanes::Transform>(entity);
}

svanes::Entity DynamicObject::GetEntity() const { return entity; }

svanes::Kinematic2D &
DynamicObject::GetKinematic(svanes::Registry &world) const {
    return world.GetComponent<svanes::Kinematic2D>(entity);
}

svanes::Transform &DynamicObject::GetTransform(svanes::Registry &world) const {
    return world.GetComponent<svanes::Transform>(entity);
}

void DynamicObject::UpdateVisuals(svanes::Registry &world) const {
    visuals.SetTransform(world, GetTransform(world));
}

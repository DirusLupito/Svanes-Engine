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
static OwnedEntity CreateTimedEntity(svanes::Registry &world,
                                     svanes::Entity gameplay_timeline) {
    OwnedEntity entity(world);
    world.AddComponent<svanes::Timeline>(entity.Get(), gameplay_timeline);
    return entity;
}

// pretty much pojo slop... for now...

DynamicObject::DynamicObject(svanes::Registry &world,
                             svanes::Entity gameplay_timeline,
                             svanes::Collider2D collider,
                             std::vector<Visual> visuals)
    : entity(CreateTimedEntity(world, gameplay_timeline)),
      visuals(world, std::move(visuals)) {
    world.AddComponent<svanes::Collider2D>(entity.Get(), std::move(collider));
    world.AddComponent<svanes::Kinematic2D>(entity.Get());
    world.AddComponent<svanes::Transform>(entity.Get());
}

svanes::Entity DynamicObject::GetEntity() const { return entity.Get(); }

svanes::Kinematic2D &
DynamicObject::GetKinematic(svanes::Registry &world) const {
    return world.GetComponent<svanes::Kinematic2D>(entity.Get());
}

svanes::Transform &DynamicObject::GetTransform(svanes::Registry &world) const {
    return world.GetComponent<svanes::Transform>(entity.Get());
}

void DynamicObject::UpdateVisuals(svanes::Registry &world) const {
    visuals.SetTransform(world, GetTransform(world));
}

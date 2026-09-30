#include "dynamic_object.hpp"

#include "../../attachment_system.hpp"

#include <svanes/registry.hpp>
#include <svanes/timeline_system.hpp>

#include <stdexcept>
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

DynamicObject::DynamicObject(svanes::Registry &world,
                             svanes::Entity gameplay_timeline,
                             DynamicObjectDefinition definition)
    : name(std::move(definition.name)),
      entity(CreateTimedEntity(world, gameplay_timeline)),
      visuals(world, std::move(definition.visuals)) {
    world.AddComponent<svanes::Collider2D>(entity.Get(),
                                           std::move(definition.collider));
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
    for (const auto &attachment : attachments) {
        attachment->UpdateVisuals(world);
    }
}

const std::string &DynamicObject::GetName() const { return name; }

void DynamicObject::AddAttachment(svanes::Registry &world,
                                  std::unique_ptr<DynamicObject> attachment,
                                  svanes::Transform transform) {
    if (!attachment) {
        throw std::invalid_argument("Cannot attach an empty dynamic object");
    }

    AttachEntity(world, attachment->GetEntity(), GetEntity(), transform);
    attachments.push_back(std::move(attachment));
}

std::vector<std::unique_ptr<DynamicObject>>
DynamicObject::DetachAttachments(svanes::Registry &world,
                                 svanes::Vector2D added_velocity) {
    for (const auto &attachment : attachments) {
        DetachAttachment(world, attachment->GetEntity(), added_velocity);
    }

    return std::exchange(attachments, {});
}

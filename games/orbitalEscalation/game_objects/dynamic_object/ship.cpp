#include "ship.hpp"

#include "../../attachment_system.hpp"
#include "../../propulsion_system.hpp"

#include <svanes/registry.hpp>

#include <utility>

Ship::Ship(svanes::Registry &world, svanes::Entity gameplay_timeline,
           ShipDefinition definition)
    : DynamicObject(world, gameplay_timeline, std::move(definition.collider),
                    std::move(definition.visuals)) {
    world.AddComponent<Propulsion>(GetEntity(), definition.max_acceleration,
                                   definition.max_angular_acceleration,
                                   definition.forward);
    world.AddComponent<PropulsionControl>(GetEntity());
    attachments.reserve(definition.attachments.size());
    for (auto &attachment : definition.attachments) {
        auto &child = attachments.emplace_back(world, gameplay_timeline,
                                               std::move(attachment.ship));
        AttachEntity(world, child.GetEntity(), GetEntity(),
                     attachment.transform);
    }
}

void Ship::UpdateVisuals(svanes::Registry &world) const {
    DynamicObject::UpdateVisuals(world);
    for (const auto &attachment : attachments) {
        attachment.UpdateVisuals(world);
    }
}

std::vector<Ship> Ship::DetachAttachments(svanes::Registry &world,
                                          svanes::Vector2D added_velocity) {
    for (const auto &attachment : attachments) {
        DetachAttachment(world, attachment.GetEntity(), added_velocity);
    }
    return std::exchange(attachments, {});
}

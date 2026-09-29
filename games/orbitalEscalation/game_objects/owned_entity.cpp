#include "owned_entity.hpp"

#include <svanes/registry.hpp>

#include <utility>

OwnedEntity::OwnedEntity(svanes::Registry &world)
    : world(&world), entity(world.CreateEntity()) {}

OwnedEntity::~OwnedEntity() {
    if (world) {
        world->DestroyEntity(entity);
    }
}

OwnedEntity::OwnedEntity(OwnedEntity &&other) noexcept
    : world(std::exchange(other.world, nullptr)), entity(other.entity) {}

OwnedEntity &OwnedEntity::operator=(OwnedEntity &&other) noexcept {

    // Clean up the current entity if it exists, then exchange the world pointer
    // with nothing and take ownership of the other entity.

    if (this != &other) {
        if (world) {
            world->DestroyEntity(entity);
        }
        world = std::exchange(other.world, nullptr);
        entity = other.entity;
    }
    return *this;
}

svanes::Entity OwnedEntity::Get() const { return entity; }

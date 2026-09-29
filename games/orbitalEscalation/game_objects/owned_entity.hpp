#pragma once

#include <svanes/entity.hpp>

namespace svanes {
class Registry;
}

/**
 * Owns an entity in a Registry and manages its lifetime through RAII.
 *
 * Game objects use OwnedEntity to keep registry entities tied to
 * the lifetime of the object that created them. When the owner is destroyed,
 * the associated entity and its components are removed from the Registry.
 */
class OwnedEntity final {
public:
    /**
     * Constructs an OwnedEntity that manages a newly created entity in the
     * specified registry.
     *
     * @param world The registry in which to create and manage the entity.
     */
    explicit OwnedEntity(svanes::Registry &world);

    /**
     * Destroys the OwnedEntity and releases the associated entity through the
     * Registry.
     */
    ~OwnedEntity();

    /**
     * Override of the copy constructor to prevent copying of OwnedEntity
     * instances. This prevents something like:
     *
     * OwnedEntity entity1(...);
     * OwnedEntity entity2 = entity1;
     */
    OwnedEntity(const OwnedEntity &) = delete;

    /**
     * Override of the copy assignment operator to prevent copying of
     * OwnedEntity instances. This prevents something like:
     *
     * OwnedEntity entity1(...);
     * OwnedEntity entity2(...);
     * entity2 = entity1;
     */
    OwnedEntity &operator=(const OwnedEntity &) = delete;

    /**
     * Move constructor for transferring ownership of the entity from another
     * OwnedEntity instance. After the move, the source instance will no longer
     * own the entity.
     *
     * @param other The other OwnedEntity instance to move from.
     */
    OwnedEntity(OwnedEntity &&other) noexcept;

    /**
     * Move assignment operator for transferring ownership of the entity from
     * another OwnedEntity instance. After the move, the source instance will no
     * longer own the entity. For example:
     *
     * OwnedEntity entity1(...);
     * OwnedEntity entity2(...);
     * entity2 = std::move(entity1); // entity2 now owns the entity.
     *
     * @param other The other OwnedEntity instance to move from.
     * @return A reference to this OwnedEntity instance after the move.
     */
    OwnedEntity &operator=(OwnedEntity &&other) noexcept;

    /**
     * Returns the entity managed by this OwnedEntity.
     *
     * @return The managed entity.
     */
    svanes::Entity Get() const;

private:
    // The registry that owns the entity.
    svanes::Registry *world;

    // The entity managed by this OwnedEntity.
    svanes::Entity entity;
};

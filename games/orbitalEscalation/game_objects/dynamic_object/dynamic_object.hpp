#pragma once

#include "../visuals.hpp"

#include <svanes/collision_system.hpp>
#include <svanes/kinematic_system.hpp>

#include <memory>
#include <span>
#include <string>

/**
 * Represents the type of a dynamic object definition. The type and name
 * together identify what to create.
 *
 * For example, a ship and a missile can have the same
 * name without referring to the same definition.
 *
 * MEMBERS:
 * - Ship: A ship definition.
 * - Missile: A missile definition.
 * - MissileLauncher: A missile launcher definition.
 */
enum class DynamicObjectType { Ship, Missile, MissileLauncher };

/**
 * Defines an object attached to another object. This is a reference to a
 * definition, not an entity that has already been created in the registry.
 *
 * FIELDS:
 * - type: The type of the attached object.
 * - name: The name of the definition to use when creating the attached object.
 * - transform: The position and rotation relative to the parent object's
 * origin.
 */
struct AttachmentDefinition {
    DynamicObjectType type;
    std::string name;
    svanes::Transform transform;
};

/**
 * Defines properties common to all dynamic objects. Each attachment refers
 * to another definition, so we do not have to repeat all of its geometry every
 * time we want to attach another copy of it.
 *
 * FIELDS:
 * - name: The name of this definition. Multiple entities can use the same
 * definition, so this is not a unique entity identifier.
 * - collider: A Collider2D that defines the object's collision layer.
 * - visuals: A vector of Visual objects that define the object's appearance in
 * the game world.
 * - attachments: The objects initially attached to this object, with their
 * positions and rotations relative to its origin.
 */
struct DynamicObjectDefinition {
    std::string name;
    svanes::Collider2D collider;
    std::vector<Visual> visuals;
    std::vector<AttachmentDefinition> attachments;
};

/**
 * Represents a dynamic object in the game world, consisting of some rendered
 * and collidable geometry, and a kinematic component for movement.
 */
class DynamicObject {
public:
    /**
     * Destroys the DynamicObject entity, its visuals, and any objects still
     * attached to it. Implemented by concrete derived classes, such as
     * ships and missiles.
     */
    virtual ~DynamicObject() = default;

    /**
     * Returns the name of the definition used to create this object. Multiple
     * objects can have this name. GetEntity distinguishes the actual entities.
     *
     * @return The name of the dynamic object definition.
     */
    const std::string &GetName() const;

    /**
     * Attaches an object and takes ownership of it. Destroying this object will
     * then also destroy the attachment, unless it has been detached first.
     *
     * @param world The registry containing both objects.
     * @param attachment The object to attach and take ownership of.
     * @param transform The position and rotation relative to this object's
     * origin.
     *
     * @throws std::invalid_argument If the attachment is null.
     */
    void AddAttachment(svanes::Registry &world,
                       std::unique_ptr<DynamicObject> attachment,
                       svanes::Transform transform);

    /**
     * Detaches all objects directly attached to this object. The returned
     * objects now own those entities, so destroying this object no longer
     * destroys them.
     *
     * @param world The registry containing the object and its attachments.
     * @param added_velocity The extra world-space velocity, in units per local
     * tic, added to the detached objects' linear velocity.
     * @return The detached objects. Empty if this object has no attachments.
     */
    std::vector<std::unique_ptr<DynamicObject>>
    DetachAttachments(svanes::Registry &world, svanes::Vector2D added_velocity);

    /**
     * Returns the objects directly attached to this object. Ownership stays
     * here, so callers cannot remove an attachment without transferring it.
     *
     * @return The directly attached objects.
     */
    std::span<const std::unique_ptr<DynamicObject>> GetAttachments() const;

    /**
     * Detaches one object and transfers its ownership to the caller.
     *
     * @param world The registry containing the objects.
     * @param child The entity of the directly attached object.
     * @param added_velocity The extra world-space velocity, in units per local
     * tic.
     * @return The detached object.
     * @throws std::invalid_argument If child is not directly attached here.
     */
    std::unique_ptr<DynamicObject> Detach(svanes::Registry &world,
                                          svanes::Entity child,
                                          svanes::Vector2D added_velocity);

    /**
     * Removes attachments whose health has reached zero, including those on
     * attached objects. Explosion processing must finish first, so for example,
     * a dead missile still has its geometry when we determine what its blast
     * hits. Any child of a dead attachment is also removed, even if it is not
     * dead itself.
     *
     * @param world The registry containing the objects and their health.
     */
    void RemoveDeadAttachments(svanes::Registry &world);

    /**
     * Returns the unique identifier of the dynamic object entity.
     *
     * @return The unique identifier of the dynamic object entity.
     */
    svanes::Entity GetEntity() const;

    /**
     * Returns a reference to the Kinematic2D component of the dynamic object
     * entity.
     *
     * @param world The registry containing the dynamic object entity and its
     * components.
     * @return A reference to the Kinematic2D component of the dynamic object
     * entity.
     */
    svanes::Kinematic2D &GetKinematic(svanes::Registry &world) const;

    /**
     * Returns a reference to the Transform component of the dynamic object
     * entity.
     *
     * @param world The registry containing the dynamic object entity and its
     * components.
     * @return A reference to the Transform component of the dynamic object
     * entity.
     */
    svanes::Transform &GetTransform(svanes::Registry &world) const;

    /**
     * Updates the visuals of the dynamic object entity to match its current
     * transform, including any attached objects. Attachment transforms should
     * be updated with UpdateAttachments first.
     *
     * @param world The registry containing the dynamic object entity and its
     * components.
     */
    void UpdateVisuals(svanes::Registry &world) const;

    // Nobody should ever construct a DynamicObject directly, only derived
    // classes should be able to do that.
protected:
    /**
     * Override of the copy constructor to prevent copying of DynamicObject
     * instances. This prevents something like:
     *
     * DynamicObject object1(...);
     * DynamicObject object2 = object1;
     */
    DynamicObject(const DynamicObject &) = delete;

    /**
     * Override of the copy assignment operator to prevent copying of
     * DynamicObject instances. This prevents something like:
     *
     * DynamicObject object1(...);
     * DynamicObject object2(...);
     * object2 = object1;
     */
    DynamicObject &operator=(const DynamicObject &) = delete;

    /**
     * Move constructor for transferring ownership of the dynamic object entity
     * and visuals from another DynamicObject instance.
     *
     * @param other The other DynamicObject instance to move from.
     */
    DynamicObject(DynamicObject &&) noexcept = default;

    /**
     * Move assignment operator for transferring ownership of the dynamic
     * object entity and visuals from another DynamicObject instance.
     *
     * @param other The other DynamicObject instance to move from.
     * @return A reference to this DynamicObject instance after the move.
     */
    DynamicObject &operator=(DynamicObject &&) noexcept = default;

    /**
     * Constructs a DynamicObject entity in the provided registry.
     *
     * @param world The registry in which to create the dynamic object entity
     * and its components.
     * @param gameplay_timeline The timeline entity that controls the dynamic
     * object's behavior in the game world.
     * @param definition The object's name, collider, and visuals. The catalog
     * creates its attachments after constructing the object itself.
     */
    DynamicObject(svanes::Registry &world, svanes::Entity gameplay_timeline,
                  DynamicObjectDefinition definition);

private:
    // The name of the catalog definition used to create this object.
    std::string name;

    // The owner of the dynamic object entity in the registry.
    OwnedEntity entity;

    // The Visuals object that manages the visual representation of the dynamic
    // object entity.
    Visuals visuals;

    // The objects directly attached to this object.
    std::vector<std::unique_ptr<DynamicObject>> attachments;
};

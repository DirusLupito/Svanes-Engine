#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/primitive_geometry.hpp>

namespace svanes {
class Registry;
}

/**
 * Attaches an entity to another entity. Its position and rotation are computed
 * from the parent, so an attached entity does not have a Kinematic2D while
 * attached. Attachments are a tree, not a graph, and an entity has exactly one
 * parent. Any cycles in the attachment tree must be detected and rejected. The
 * root of the tree is a non-attached entity.
 *
 * FIELDS:
 * - parent: An identifier for the entity whose transform this attachment
 * follows.
 * - transform: The position and rotation relative to the parent's origin.
 */
struct Attachment {
    svanes::Entity parent;
    svanes::Transform transform;
};

/**
 * Updates the world transforms of attached entities, resolving parents before
 * their children. This should run before updating the objects' visuals.
 *
 * @param world The registry containing the attachments and their parents.
 * @throws std::invalid_argument If a transform is missing, parenting contains
 * a cycle, or an attached entity has independent kinematic movement.
 */
void UpdateAttachments(svanes::Registry &world);

/**
 * Attaches an entity at a position and rotation relative to its parent.
 * Removes its Kinematic2D so physics does not move it independently.
 * Will not affect the ownership of the entity. Destroying the parent
 * will not destroy the attached entity unless the attached entity's
 * lifetime is tied to the parent's lifetime through some other means.
 * Furthermore, destroying the parent will not automatically detach
 * the attached entity.
 *
 * @param world The registry containing the entities.
 * @param entity The entity to attach.
 * @param parent The entity to attach it to.
 * @param transform The position and rotation relative to the parent's origin.
 * @throws std::invalid_argument If the transform is not finite, required
 * transforms are missing, or parenting would contain a cycle.
 */
void AttachEntity(svanes::Registry &world, svanes::Entity entity,
                  svanes::Entity parent, svanes::Transform transform);

/**
 * Detaches an entity without changing its world position or rotation. Its
 * Kinematic2D starts with the moving ancestor's linear velocity plus the
 * supplied velocity. It does not inherit acceleration or angular velocity.
 *
 * @param world The registry containing the attachment and its parent.
 * @param entity The attached entity to detach.
 * @param added_velocity The extra world-space velocity, in units per local tic.
 */
void DetachAttachment(svanes::Registry &world, svanes::Entity entity,
                      svanes::Vector2D added_velocity);

/**
 * Finds the root of an attachment tree.
 *
 * @param world The registry containing the attachment tree. UpdateAttachments
 * should have validated the tree before it is used for collision or launching.
 * @param entity The entity whose root we want.
 *
 * @return The root entity, or entity itself if it is not attached.
 */
svanes::Entity GetAttachmentRoot(const svanes::Registry &world,
                                 svanes::Entity entity);

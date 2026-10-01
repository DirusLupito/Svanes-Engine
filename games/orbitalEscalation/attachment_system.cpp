#include "attachment_system.hpp"

#include <svanes/kinematic_system.hpp>
#include <svanes/registry.hpp>

#include <cmath>
#include <stdexcept>
#include <unordered_map>

/**
 * Represents the visitation state of an entity during attachment updates.
 *
 * MEMBERS:
 * - Visiting: The entity is currently being visited in the update process.
 * - Complete: The entity has been fully processed and its transform has been
 *   updated.
 */
enum class AttachmentVisit { Visiting, Complete };

/**
 * Updates the world transform of an attached entity, resolving its parent's
 * transform first. This function is called recursively to ensure that all
 * ancestors are processed before their children.
 *
 * @param world The registry containing the attachments and their parents.
 * @param entity The attached entity whose transform is being updated.
 * @param visits A map tracking the visitation state of entities during the
 * update process.
 * @throws std::invalid_argument If a transform is missing, parenting contains
 * a cycle, or an attached entity has independent kinematic movement.
 */
static void
UpdateAttachment(svanes::Registry &world, svanes::Entity entity,
                 std::unordered_map<svanes::Entity, AttachmentVisit> &visits) {

    // Ensure that the attachments form a tree, not a graph.


    if (const auto found = visits.find(entity); found != visits.end()) {
        if (found->second == AttachmentVisit::Visiting) {
            throw std::invalid_argument(
                "Attachment parenting contains a cycle");
        }
        return;
    }

    visits.emplace(entity, AttachmentVisit::Visiting);
    const auto &attachment = world.GetComponent<Attachment>(entity);


    // Every attachment will have a parent, this function should never
    // be called on a root entity.

    if (!world.HasComponent<svanes::Transform>(entity) ||
        !world.HasComponent<svanes::Transform>(attachment.parent)) {
        throw std::invalid_argument("Attachment and parent require transforms");
    }

    if (world.HasComponent<svanes::Kinematic2D>(entity)) {
        throw std::invalid_argument(
            "An attached entity cannot move independently");
    }

    // Recursive case: if the parent is also an attachment, update it first to
    // ensure that the parent's world transform is computed before computing the
    // child's world transform.

    if (world.HasComponent<Attachment>(attachment.parent)) {
        UpdateAttachment(world, attachment.parent, visits);
    }

    // Base case: we update the world transform of the attached entity by
    // composing its parent's world transform with its local transform.

    world.GetComponent<svanes::Transform>(entity) = svanes::ComposeTransforms(
        world.GetComponent<svanes::Transform>(attachment.parent),
        attachment.transform);
    visits.at(entity) = AttachmentVisit::Complete;
}

void UpdateAttachments(svanes::Registry &world) {
    std::unordered_map<svanes::Entity, AttachmentVisit> visits;
    world.ForEach<Attachment>([&](svanes::Entity entity, const Attachment &) {
        UpdateAttachment(world, entity, visits);
    });
}

void AttachEntity(svanes::Registry &world, svanes::Entity entity,
                  svanes::Entity parent, svanes::Transform transform) {
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y) ||
        !std::isfinite(transform.rotation)) {
        throw std::invalid_argument("Attachment transform must be finite");
    }

    if (!world.HasComponent<svanes::Transform>(entity) ||
        !world.HasComponent<svanes::Transform>(parent)) {
        throw std::invalid_argument("Attachment and parent require transforms");
    }

    UpdateAttachments(world);
    auto ancestor = parent;

    // ensure attachments are a tree, not a graph

    while (true) {
        if (ancestor == entity) {
            throw std::invalid_argument(
                "Attachment parenting contains a cycle");
        }
        if (!world.HasComponent<Attachment>(ancestor)) {
            break;
        }
        ancestor = world.GetComponent<Attachment>(ancestor).parent;
    }

    world.AddComponent<Attachment>(entity, Attachment{parent, transform});
    world.RemoveComponent<svanes::Kinematic2D>(entity);
    world.GetComponent<svanes::Transform>(entity) = svanes::ComposeTransforms(
        world.GetComponent<svanes::Transform>(parent), transform);
}

void DetachAttachment(svanes::Registry &world, svanes::Entity entity,
                      svanes::Vector2D added_velocity) {
    if (!std::isfinite(added_velocity.x) || !std::isfinite(added_velocity.y)) {
        throw std::invalid_argument(
            "Attachment launch velocity must be finite");
    }
    UpdateAttachments(world);

    // We need to find the root parent of the detached entity, as
    // no attached entity will have a Kinematic2D component.

    const auto parent = GetAttachmentRoot(world, entity);

    svanes::Kinematic2D motion;
    if (world.HasComponent<svanes::Kinematic2D>(parent)) {
        const auto &parent_motion =
            world.GetComponent<svanes::Kinematic2D>(parent);
        motion.velocity_x = parent_motion.velocity_x;
        motion.velocity_y = parent_motion.velocity_y;
    }

    motion.velocity_x += added_velocity.x;
    motion.velocity_y += added_velocity.y;
    world.AddComponent<svanes::Kinematic2D>(entity, motion);
    world.RemoveComponent<Attachment>(entity);
}

svanes::Entity GetAttachmentRoot(const svanes::Registry &world,
                                 svanes::Entity entity) {
    while (world.HasComponent<Attachment>(entity)) {
        entity = world.GetComponent<Attachment>(entity).parent;
    }
    return entity;
}

#pragma once

#include "owned_entity.hpp"

#include <svanes/vector2d.hpp>

#include <span>
#include <vector>

/**
 * Creates the invisible collision boxes that delete gameplay objects
 * at the world's boundaries.
 *
 * @param world The registry in which to create the boxes' colliders.
 * @return The owners of the boxes' collision entities.
 */
std::vector<OwnedEntity> CreateDeletionBoxes(svanes::Registry &world);

/**
 * Marks collidable objects touching a deletion box as dead so their owners
 * remove them, including objects that do not otherwise have health.
 *
 * @param world The registry containing the objects to check.
 * @param boxes The deletion boxes whose colliders define the world boundaries.
 *
 * @return The positions of objects newly marked for removal, where the client
 * can display collision flashes.
 */
std::vector<svanes::Vector2D>
ApplyDeletionBoxes(svanes::Registry &world, std::span<const OwnedEntity> boxes);

#pragma once

#include "owned_entity.hpp"

#include <svanes/vector2d.hpp>

#include <vector>

/**
 * Creates the large red boxes that show where gameplay objects are deleted.
 *
 * @param world The client's registry in which to create the boxes' visuals.
 * @return The owners of the boxes' visual entities.
 */
std::vector<OwnedEntity> CreateDeletionBoxes(svanes::Registry &world);

/**
 * Marks collidable objects touching a deletion box as dead so their owners
 * remove them, including objects that do not otherwise have health.
 *
 * @param world The registry containing the objects to check.
 * @return The positions of objects newly marked for removal, where the client
 * can display collision flashes.
 */
std::vector<svanes::Vector2D> ApplyDeletionBoxes(svanes::Registry &world);

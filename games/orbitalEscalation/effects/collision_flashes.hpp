#pragma once

#include "../game_objects/owned_entity.hpp"
#include <svanes/vector2d.hpp>
#include <vector>

/**
 * Updates all active collision flashes, expanding them at the start of their
 * lifetime and fading them out before destroying them when their lifetime has
 * elapsed.
 *
 * @param world The registry containing the collision flash entities.
 * @param collision_flashes The list of active collision flash entities.
 */
void UpdateCollisionFlashes(svanes::Registry &world,
                            std::vector<OwnedEntity> &collision_flashes);

/**
 * Creates a collision flash at the specified point and adds it to the list of
 * active flashes.
 *
 * Just a rapidly expanding ball that's much brighter at its center than at its
 * edge. A ball that will add white to the screen where it is drawn, at greater
 * intensity at its center than at its edge. Will fade out much slower than it
 * expands. That is how we get the effect of an explosion of light.
 * Its Timeline supplies the elapsed lifetime; there is no second accumulator.
 *
 * @param world The registry to create the collision flash entity in.
 * @param gameplay_timeline The parent timeline entity to which the new flash's
 * Timeline will be linked.
 * @param collision_flashes The list to which the new collision flash will be
 * added.
 * @param contact_point The world-space point at which the collision flash will
 * be drawn.
 */
void CreateCollisionFlash(svanes::Registry &world,
                          svanes::Entity gameplay_timeline,
                          std::vector<OwnedEntity> &collision_flashes,
                          svanes::Vector2D contact_point);

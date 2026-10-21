#pragma once

#include "owned_entity.hpp"

/**
 * Creates the stationary green triangle and its repulsion field
 * at its fixed position beside the planet.
 *
 * @param world The registry in which to create the triangle.
 *
 * @return The owner of the triangle's entity.
 */
OwnedEntity CreateRepellingTriangle(svanes::Registry &world);

/**
 * Creates the gray square at its fixed position beside the planet.
 *
 * @param world The registry in which to create the square.
 * @param gameplay_timeline The master clock controlling the square's motion.
 *
 * @return The owner of the square's entity.
 */
OwnedEntity CreateRotatingSquare(svanes::Registry &world,
                                 svanes::Entity gameplay_timeline);

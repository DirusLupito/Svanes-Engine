#pragma once

#include <svanes/entity.hpp>
#include <svanes/vector2d.hpp>

namespace svanes {

class Registry;

}

/** The width and height of every bullet's square body. */
inline constexpr float BulletSize = 8.0F;

/**
 * Component marking an entity as a projectile, along with the entity that
 * fired it. Bullets ignore collisions with their owner so they do not strike
 * the shooter they spawn on top of, and pass through anything their owner
 * spawned.
 *
 * FIELDS:
 * - owner: The entity that fired this bullet.
 */
struct Bullet {
    svanes::Entity owner;
};

/**
 * Creates a bullet moving outward from an origin. Its Timeline follows the
 * same clock as its owner's, so it pauses and changes speed with its owner.
 *
 * @param world The registry the bullet entity is created in.
 * @param owner The entity firing the bullet, which must have a Timeline.
 * @param origin The world position the bullet starts at.
 * @param direction The direction of travel. Need not be normalized.
 * @param speed The travel speed in world units per second.
 *
 * @throws std::invalid_argument if the direction is not finite or is zero
 * length.
 */
void SpawnBullet(svanes::Registry &world, svanes::Entity owner,
                 svanes::Vector2D origin, svanes::Vector2D direction,
                 float speed);

/**
 * @param world The registry containing the bullet and the other entity.
 * @param bullet The bullet.
 * @param other An entity the bullet overlaps.
 * @return Whether the bullet passes through the other entity rather than
 * striking it, which it does for its owner, any minion its owner spawned, and
 * other bullets.
 */
bool PassesThrough(const svanes::Registry &world, const Bullet &bullet,
                   svanes::Entity other);

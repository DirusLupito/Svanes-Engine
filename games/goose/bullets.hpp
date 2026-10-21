#pragma once

#include <svanes/entity.hpp>
#include <svanes/vector2d.hpp>

#include <vector>

namespace svanes {

class Registry;

}

/** The width and height of every bullet's square body. */
inline constexpr float BulletSize = 8.0F;

/**
 * Component marking an entity as a projectile, along with the entity that
 * fired it. Bullets ignore collisions with their owner so they do not strike
 * the shooter they spawn on top of.
 *
 * FIELDS:
 * - owner: The entity that fired this bullet.
 */
struct Bullet {
    svanes::Entity owner;
};

/**
 * A single bullet striking something. The bullet is already destroyed.
 *
 * FIELDS:
 * - target: The entity the bullet struck.
 * - direction: The bullet's velocity at the moment of impact, so the target
 *   can be pushed along the bullet's line of travel.
 */
struct BulletHit {
    svanes::Entity target;
    svanes::Vector2D direction;
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
 * Destroys every bullet that overlaps something with a Collider2D other than
 * its owner or another bullet, and reports what each one struck. Hits are
 * reported, not applied, so the caller decides what being hit does to a target.
 * The arena is closed on every side, so every bullet eventually strikes
 * something.
 *
 * @param world The registry containing the bullets and their targets.
 * @return One BulletHit per bullet that struck something.
 */
std::vector<BulletHit> UpdateBullets(svanes::Registry &world);

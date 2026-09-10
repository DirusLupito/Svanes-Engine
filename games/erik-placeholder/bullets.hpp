#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <vector>

namespace svanes {

class Registry;

}

/**
 * Tag component marking an entity as a projectile, along with the entity that
 * fired it. Bullets ignore collisions with their owner so they do not strike the
 * shooter they spawn on top of.
 *
 * FIELDS:
 * - owner: The entity that fired this bullet. Collisions against it are ignored.
 */
struct Bullet {
    svanes::Entity owner = 0;
};

/**
 * A single bullet-versus-entity collision reported back to the game. The bullet
 * entity is destroyed before the hit is reported.
 *
 * FIELDS:
 * - target: The entity the bullet struck.
 * - owner: The entity that fired the bullet.
 * - direction: The bullet's velocity at the moment of impact, used by the game
 *   to push the target along the bullet's line of travel.
 */
struct BulletHit {
    svanes::Entity target = 0;
    svanes::Entity owner = 0;
    svanes::Vector2D direction;
};

/**
 * Creates a bullet entity moving outward from an origin. The direction is
 * normalized internally, so callers may pass any nonzero vector, such as the raw
 * offset from a shooter to the point it is aiming at.
 *
 * @param world The registry the bullet entity is created in.
 * @param owner The entity firing the bullet, recorded so it cannot hit itself.
 * @param origin The world position the bullet starts at.
 * @param direction The direction of travel. Need not be normalized.
 * @param speed The travel speed in world units per second.
 * @param color The color the bullet is drawn in.
 *
 * @throws std::invalid_argument if the direction is not finite or is zero length.
 * @throws std::invalid_argument if the speed is not finite or is not positive.
 */
void SpawnBullet(
    svanes::Registry& world, svanes::Entity owner,
    svanes::Vector2D origin, svanes::Vector2D direction,
    float speed, svanes::Color color
);

/**
 * Advances every bullet in the world for one frame, destroying those that left the
 * bounds or struck something, and returns what they struck.
 *
 * Hits are reported, not applied. The caller decides what being hit does to a
 * target, so the same bullet can knock the goose backwards and damage the enemy.
 *
 * @param world The registry containing the bullets and everything they may hit.
 * @param bounds The world region bullets remain alive inside. Bullets outside it
 * are destroyed without reporting a hit.
 *
 * @return One BulletHit per bullet that struck something this frame.
 */
std::vector<BulletHit> UpdateBullets(svanes::Registry& world, const svanes::Rectangle2D& bounds);

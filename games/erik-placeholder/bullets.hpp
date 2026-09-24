#pragma once

#include <svanes/entity.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/collision_system.hpp>
#include <svanes/render/render_system.hpp>

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
 * - id: The simulation's shot sequence number, or zero before assignment.
 * - owner_key: The firing peer's id, with zero identifying the enemy.
 */
struct Bullet {
    svanes::Entity owner = 0;
    std::uint64_t id = 0;
    std::uint32_t owner_key = 0;
};

/**
 * Stores a shot independently of its temporary registry entity.
 * FIELDS:
 * - bullet: The shot's identity and owner.
 * - transform: Its position and orientation.
 * - motion: Its velocity and acceleration.
 * - timeline: Its elapsed simulation time.
 * - shape: Its rendered body.
 * - collider: Its collision body.
 */
struct BulletSnapshot {
    Bullet bullet;
    svanes::Transform transform;
    svanes::Kinematic2D motion;
    svanes::Timeline timeline;
    svanes::SolidShape shape;
    svanes::Collider2D collider;
};

/**
 * @param world The registry containing the shots.
 * @return Bullet entities in shot order, using local creation order for unnumbered shots.
 */
std::vector<svanes::Entity> OrderedBullets(const svanes::Registry& world);

/**
 * @param world The registry containing the shots.
 * @return All live shots in simulation order.
 */
std::vector<BulletSnapshot> CaptureBullets(const svanes::Registry& world);

/**
 * Replaces live shots with the saved set, including shots destroyed since capture.
 * @param world The registry containing the shots and their unchanged owner entities.
 * @param snapshots The shots to recreate in simulation order.
 */
void RestoreBullets(svanes::Registry& world, const std::vector<BulletSnapshot>& snapshots);

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
 * Checks bullets after physics, destroying those that left the bounds or struck
 * something, and returns what they struck. Shots are checked in simulation order
 * and collision targets in entity order so replay resolves competing hits consistently.
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

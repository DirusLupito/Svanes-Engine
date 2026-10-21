#pragma once

#include <svanes/entity.hpp>
#include <svanes/timeline_system.hpp>

#include <cstdint>

namespace svanes {

class Registry;

}

/** The width and height of the enemy's square body. */
inline constexpr float EnemySize = 120.0F;

/**
 * The enemy on the server. It sweeps back and forth above the arena on a path
 * set by its Timeline's total time and fires straight down. It moves by writing
 * its Transform directly rather than through Kinematic2D, so gravity does not
 * apply.
 *
 * Once enough bullets hit it, it dies: its collider is removed and it stops
 * acting. The entity stays, so bullets it fired still know their owner.
 */
class Enemy final {
public:
    /**
     * Creates the enemy's entity at the center of its path.
     *
     * @param world The registry the entity is created in.
     * @param clock The entity whose Timeline the enemy's Timeline follows.
     */
    Enemy(svanes::Registry &world, svanes::Entity clock);

    /**
     * Moves along the path and fires by the enemy's Timeline delta. Does
     * nothing once the enemy is dead.
     *
     * @param world The registry holding the enemy.
     */
    void Advance(svanes::Registry &world);

    /**
     * Takes one hit, dying when no hits remain. Does nothing once the enemy is
     * dead.
     *
     * @param world The registry holding the enemy.
     */
    void Hit(svanes::Registry &world);

    /** @return Whether the enemy has hits remaining. */
    bool IsAlive() const;

    /**
     * @return The enemy's entity, for looking up its components or comparing
     * against the target or owner of a bullet.
     */
    svanes::Entity GetEntity() const;

private:
    svanes::Entity entity;
    std::uint32_t hits_remaining;
    svanes::TicCount fire_cooldown = 0;
};

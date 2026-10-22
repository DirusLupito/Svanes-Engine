#pragma once

#include <svanes/entity.hpp>
#include <svanes/timeline_system.hpp>

#include <cstdint>
#include <random>

namespace svanes {

class Registry;

}

/** The width and height of the enemy's square body. */
inline constexpr float EnemySize = 120.0F;

/** The number of hits the enemy takes to die. */
inline constexpr std::uint32_t EnemyMaxHealth = 20;

/**
 * The enemy on the server. It sweeps back and forth above the arena on a path
 * set by its Timeline's total time and fires straight down. It moves by writing
 * its Transform directly rather than through Kinematic2D, so gravity does not
 * apply. Every so often it drops a group of minions, at random intervals and in
 * random numbers, half of them archers, rounded down.
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
     * Moves along the path, fires, and drops minions by the enemy's Timeline
     * delta. Does nothing once the enemy is dead.
     *
     * @param world The registry holding the enemy.
     * @param spawning Whether minions may be dropped. While false, the wait
     * until the next group is held where it is.
     */
    void Advance(svanes::Registry &world, bool spawning);

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
     * @return The hits the enemy can still take, from zero up to
     * EnemyMaxHealth.
     */
    std::uint32_t GetHealth() const;

    /**
     * @return The enemy's entity, for looking up its components or comparing
     * against the target or owner of a bullet.
     */
    svanes::Entity GetEntity() const;

private:
    /** @return A random wait before the next group of minions. */
    svanes::TicCount RollSpawnDelay();

    svanes::Entity entity;
    std::uint32_t health;
    svanes::TicCount fire_cooldown = 0;
    std::mt19937 random;
    svanes::TicCount spawn_cooldown;
};

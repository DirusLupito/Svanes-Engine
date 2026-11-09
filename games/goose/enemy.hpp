#pragma once

#include <svanes/entity.hpp>
#include <svanes/timeline_system.hpp>

#include <cstdint>
#include <random>

namespace svanes {

class EventManager;
class Registry;

} // namespace svanes

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
 *
 * It never creates its minions itself. It raises a SpawnEvent for each one.
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
     * @param events The event manager to raise a SpawnEvent on for each minion
     * dropped.
     * @param spawning Whether minions may be dropped. While false, the wait
     * until the next group is held where it is.
     */
    void Advance(svanes::Registry &world, svanes::EventManager &events,
                 bool spawning);

    /**
     * Takes one hit. Does nothing once the enemy is dead. Taking the last hit
     * only drops its health to zero; the caller raises the death, and Die
     * handles it.
     *
     * @return Whether this took the enemy's last hit.
     */
    bool Hit();

    /**
     * Handles the enemy's death by removing its collider, so nothing touches
     * it any more.
     *
     * @param world The registry holding the enemy.
     */
    void Die(svanes::Registry &world);

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
    svanes::Entity clock;
    std::uint32_t health;
    svanes::TicCount fire_cooldown = 0;
    std::mt19937 random;
    svanes::TicCount spawn_cooldown;
};

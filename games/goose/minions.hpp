#pragma once

#include <svanes/entity.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <span>

namespace svanes {

class Registry;

}

/** The width of a melee minion's body, wider than an archer's. */
inline constexpr float MeleeMinionWidth = 30.0F;

/** The height of a melee minion's body, shorter than an archer's. */
inline constexpr float MeleeMinionHeight = 42.0F;

/** The width of an archer minion's body, thinner than a melee minion's. */
inline constexpr float ArcherMinionWidth = 20.0F;

/** The height of an archer minion's body, taller than a melee minion's. */
inline constexpr float ArcherMinionHeight = 54.0F;

/**
 * How much wider a minion's collider is than its body. Minions push each other
 * apart by their colliders, so this is the gap left between neighbors.
 */
inline constexpr float MinionSpacing = 10.0F;

/** The health a minion spawns with. */
inline constexpr std::uint32_t MinionMaxHealth = 3;

/**
 * How a minion fights.
 *
 * MEMBERS:
 * - Melee: Walks up to the closest goose and hurts it by touch.
 * - Archer: Keeps its distance from the closest goose and shoots at it.
 */
enum class MinionKind : std::uint8_t {
    Melee,
    Archer,
};

/**
 * Component marking an entity as a minion, along with the entity that spawned
 * it. Bullets fired by the spawner pass through its minions, and archers fire
 * bullets owned by their spawner, so those pass through every minion too and
 * count as the spawner's.
 *
 * FIELDS:
 * - spawner: The entity that spawned this minion.
 * - kind: How the minion fights.
 * - health: The health remaining. The minion is destroyed when it reaches
 *   zero.
 * - fire_cooldown: The time until an archer may fire again. Unused by melee
 *   minions.
 */
struct Minion {
    svanes::Entity spawner;
    MinionKind kind;
    std::uint32_t health = MinionMaxHealth;
    svanes::TicCount fire_cooldown = 0;
};

/**
 * Creates a minion under gravity. Its Timeline follows the same clock as its
 * spawner's, so it pauses and changes speed with its spawner.
 *
 * @param world The registry the minion entity is created in.
 * @param spawner The entity spawning the minion, which must have a Timeline.
 * @param kind How the minion fights.
 * @param position The world position the minion starts at.
 */
void SpawnMinion(svanes::Registry &world, svanes::Entity spawner,
                 MinionKind kind, svanes::Vector2D position);

/**
 * Pushes every minion out of the Solids it overlaps and away from other
 * minions of its kind, then moves it relative to whichever target is closest.
 * Melee minions walk up to it, and archers keep their distance and shoot at it
 * when in range. With no targets, minions stand still. Call after the engine's
 * physics step has moved them.
 *
 * @param world The registry containing the minions and the arena.
 * @param targets The world positions minions may go after.
 */
void AdvanceMinions(svanes::Registry &world,
                    std::span<const svanes::Vector2D> targets);

/**
 * Takes health from a minion, destroying it once none remains.
 *
 * @param world The registry containing the minion.
 * @param entity The minion's entity.
 * @param damage The health to take. Health stops at zero.
 */
void DamageMinion(svanes::Registry &world, svanes::Entity entity,
                  std::uint32_t damage);

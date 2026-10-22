#pragma once

#include <svanes/entity.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>

namespace svanes {

class Registry;

}

/** The width of a goose's body: its 29 pixel wide frame drawn at 1.5x. */
inline constexpr float GooseWidth = 43.5F;

/** The height of a goose's body: its 27 pixel tall frame drawn at 1.5x. */
inline constexpr float GooseHeight = 40.5F;

/** The health a goose spawns with, and is restored to when it respawns. */
inline constexpr std::uint32_t GooseMaxHealth = 5;

/**
 * What the goose should attempt over a single step. The client fills this in
 * from the keyboard and mouse, so the goose works in terms of what the player
 * wants to do rather than which keys are down.
 *
 * FIELDS:
 * - move: Walk direction: -1 walks left, +1 walks right, 0 stands still.
 * - dash: Dash direction, as -1 for left, +1 for right, and 0 for no dash.
 *   Ignored while a dash is already running or on cooldown.
 * - jump: Whether the jump input is held. Launches a jump from the ground, and
 *   sustains flight in the air for as long as flight time remains.
 * - fire: Whether the fire input is held. Firing is rate limited internally, so
 *   holding it produces a steady stream rather than one bullet per step.
 * - aim: The direction bullets are fired toward, as an offset from the goose's
 *   position.
 */
struct GooseIntent {
    float move = 0.0F;
    float dash = 0.0F;
    bool jump = false;
    bool fire = false;
    svanes::Vector2D aim{};
};

/**
 * Which animation the goose is showing. The server chooses it each step from
 * what the goose is doing, and clients draw it.
 *
 * MEMBERS:
 * - Idle: Standing still.
 * - Walking: Moving horizontally.
 * - Flying: Airborne under held jump.
 */
enum class GooseState : std::uint8_t {
    Idle,
    Walking,
    Flying,
};

/**
 * Who a hit on a goose came from. Each has its own invincibility window, so
 * being hit by one does not protect the goose from the other.
 *
 * MEMBERS:
 * - Enemy: The enemy or anything it spawned.
 * - Player: Another player's goose.
 */
enum class HitSource : std::uint8_t {
    Enemy,
    Player,
};

/**
 * The player character on the server. Owns one entity and moves it by writing
 * to the engine's Kinematic2D component, letting Gravity and the engine's
 * physics step do the integration.
 *
 * The goose can walk, jump, dash along the ground, and fly for a limited time
 * that refills whenever it lands. It fires bullets toward an aim point, and
 * takes knockback from bullets and from touching the enemy. It has health, and
 * once that runs out it lies dead, ignoring input and hits, until it returns to
 * the spawn point it was created at.
 */
class Goose final {
public:
    /**
     * Creates the goose's entity.
     *
     * @param world The registry the entity is created in.
     * @param clock The entity whose Timeline the goose's Timeline follows.
     * @param position The world position to spawn at, and to respawn at after
     * dying.
     */
    Goose(svanes::Registry &world, svanes::Entity clock,
          svanes::Vector2D position);

    /**
     * Applies input and advances timers by the goose's Timeline delta. Call
     * after the engine's physics step has moved the goose. While dead, input is
     * ignored, and the goose respawns once its respawn timer runs out.
     *
     * @param world The registry containing the goose.
     * @param intent The input for this step.
     */
    void Advance(svanes::Registry &world, const GooseIntent &intent);

    /**
     * Throws the goose along a direction, takes damage, and starts the
     * knockback timer and the source's invincibility timer. Player control is
     * suspended until the knockback timer expires, and further hits from the
     * same source are ignored until its invincibility timer does. Ignored
     * entirely while dead. If this takes the last of the goose's health, the
     * goose dies.
     *
     * @param world The registry holding the goose entity.
     * @param direction The direction to be thrown in. Need not be normalized.
     * @param speed The speed to be thrown at, in world units per second.
     * @param damage The health to lose. Health stops at zero.
     * @param source Who the hit came from.
     *
     * @throws std::invalid_argument if the direction is not finite or is zero
     * length.
     */
    void Hit(svanes::Registry &world, svanes::Vector2D direction, float speed,
             std::uint32_t damage, HitSource source);

    /**
     * Kills the goose, even while it is invincible. Does nothing if it is
     * already dead, so its respawn timer is not restarted.
     */
    void Kill();

    /** @return Whether the goose has no health left. */
    bool IsDead() const;

    /** @return The goose's health, from zero up to GooseMaxHealth. */
    std::uint32_t GetHealth() const;

    /**
     * @return The goose's entity, for looking up its components or comparing
     * against the target of a bullet hit.
     */
    svanes::Entity GetEntity() const;

    /** @return The animation the goose is showing. */
    GooseState GetState() const;

private:
    /** Drops health to zero and starts the respawn timer. */
    void Die();

    /**
     * Returns the goose to the spawn point it was created at, at rest, with
     * full health and flight time and every timer cleared.
     *
     * @param world The registry holding the goose entity.
     */
    void Respawn(svanes::Registry &world);

    svanes::Entity entity;
    svanes::Vector2D spawn;
    std::uint32_t health;

    GooseState state = GooseState::Idle;

    // whether the goose is standing on solid geometry, recalculated every frame
    // from the collision normals in ResolveSolidCollisions
    bool grounded = false;

    svanes::TicCount fly_time_remaining = 0;

    // counts down while a dash is running, during which horizontal input is ignored
    svanes::TicCount dash_timer = 0;

    svanes::TicCount dash_cooldown = 0;
    svanes::TicCount fire_cooldown = 0;

    // counts down while the goose is being knocked back, during which it ignores
    // player input so a hit cannot be immediately walked off
    svanes::TicCount knockback_timer = 0;

    // count down after a hit from each source, during which further hits from
    // that source are ignored
    svanes::TicCount enemy_invincible_timer = 0;
    svanes::TicCount player_invincible_timer = 0;

    // counts down while dead, and the goose respawns when it reaches zero
    svanes::TicCount respawn_timer = 0;
};

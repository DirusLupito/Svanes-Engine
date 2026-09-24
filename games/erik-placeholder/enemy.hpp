#pragma once

#include "game_components.hpp"

#include <svanes/entity.hpp>
#include <svanes/geometry/geometry.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>
#include <svanes/timeline_system.hpp>

namespace svanes {

struct FrameContext;
class Registry;

}

/**
 * What the enemy should attempt over a single frame, filling the same role for the
 * enemy that GooseIntent does for the goose. The game decides where the enemy is
 * headed and what it shoots at, and the enemy carries it out.
 *
 * Movement is given as a destination rather than a direction, since the game drives
 * the enemy along a path and already knows the point it should be at this frame.
 *
 * FIELDS:
 * - move_to: The world position to move toward, at up to move_speed per second.
 * - fire: Whether to fire this frame. Firing is rate limited internally by
 *   fire_interval.
 * - aim_point: The world position bullets are fired toward.
 */
struct EnemyIntent {
    svanes::Vector2D move_to;
    bool fire = false;
    svanes::Vector2D aim_point;
};

/**
 * Captures the enemy's movement, health, and firing timer for replay.
 * FIELDS:
 * - transform: The enemy's position and orientation.
 * - health: Its current and maximum hit points.
 * - fire_cooldown: Timeline tics remaining before another shot.
 * - alive: Whether the enemy is visible, collidable, and able to act.
 */
struct EnemySnapshot {
    svanes::Transform transform;
    Health health;
    svanes::TicCount fire_cooldown = 0;
    bool alive = true;
};

/**
 * A damageable enemy that moves toward a point and shoots. It moves by writing its
 * Transform directly rather than through Kinematic2D, so gravity does not apply and
 * it follows whatever path the game hands it.
 *
 * When Health runs out, its render and collision components are removed. The
 * entity stays reserved so rollback can revive it without changing shot ownership.
 */
class Enemy final {
public:
    /**
     * Creates the enemy entity as a solid colored square with a collider and health.
     *
     * @param world The registry the enemy entity is created in.
     * @param position The world position to spawn at.
     * @param size The width and height of the enemy's square body.
     * @param health The hit points to spawn with.
     *
     * @throws std::logic_error if the enemy is already alive.
     * @throws std::invalid_argument if the size is not finite or is not positive.
     * @throws std::invalid_argument if the health is not finite or is not positive.
     */
    void Spawn(svanes::Registry& world, svanes::Vector2D position, float size, float health);

    /**
     * Advances the enemy by one frame, stepping it toward the intended position and
     * firing if it is due. Does nothing if the enemy is dead.
     *
     * @param frame The frame context supplying the registry and the frame delta.
     * @param intent What the enemy should attempt this frame.
     */
    void Update(const svanes::FrameContext& frame, const EnemyIntent& intent);

    /**
     * Moves and fires using an explicit elapsed time, including during replay.
     * @param world The registry holding the enemy.
     * @param intent The destination and firing controls for this step.
     * @param delta_tics The elapsed simulation time in timeline tics.
     */
    void Advance(svanes::Registry& world, const EnemyIntent& intent, svanes::TicCount delta_tics);

    /**
     * @param world The registry holding the enemy.
     * @return The enemy's state at this simulation boundary, including death.
     * @throws std::logic_error if the enemy has not been spawned.
     */
    EnemySnapshot Capture(const svanes::Registry& world) const;

    /**
     * Restores the enemy, reviving or hiding it without changing its entity identity.
     * @param world The registry holding the enemy.
     * @param snapshot The earlier state belonging to this enemy.
     * @throws std::logic_error if the enemy has not been spawned.
     */
    void Restore(svanes::Registry& world, const EnemySnapshot& snapshot);

    /**
     * Subtracts from the enemy's health, hiding it and removing collision if it reaches
     * zero. Does nothing if the enemy is already dead.
     *
     * @param world The registry holding the enemy entity.
     * @param amount The hit points to remove.
     *
     * @throws std::invalid_argument if the amount is not finite or is negative.
     */
    void ApplyDamage(svanes::Registry& world, float amount);

    /**
     * @param world The registry holding the enemy entity.
     *
     * @return The enemy's current world position.
     *
     * @throws std::logic_error if the enemy is dead.
     */
    svanes::Vector2D Position(svanes::Registry& world) const;

    /**
     * @return Whether the enemy still has health and a live entity.
     */
    bool IsAlive() const;

    /**
     * @return The enemy's entity, for looking up its components or comparing
     * against the target of a bullet hit.
     */
    svanes::Entity GetEntity() const;

private:
    svanes::Entity entity = 0;
    float move_speed = 700.0F;
    svanes::TicCount fire_interval = 800000;
    svanes::TicCount fire_cooldown = 0;
    float bullet_speed = 600.0F;
    bool alive = false;
    bool spawned = false;
    float body_size = 0.0F;
};

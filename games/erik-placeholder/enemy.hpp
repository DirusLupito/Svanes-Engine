#pragma once

#include "game_components.hpp"

#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

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
 * A damageable enemy that moves toward a point and shoots. It moves by writing its
 * Transform directly rather than through Kinematic2D, so gravity does not apply and
 * it follows whatever path the game hands it.
 *
 * The enemy is destroyed once its Health runs out, after which it is no longer
 * alive and must not be asked for its position.
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
     * Subtracts from the enemy's health, destroying its entity if that brings it to
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
     * @throws std::logic_error if the enemy is dead, since its entity is gone.
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
    float fire_interval = 0.8F;
    float fire_cooldown = 0.0F;
    float bullet_speed = 600.0F;
    bool alive = false;
};

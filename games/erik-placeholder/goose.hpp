#pragma once

#include "game_components.hpp"

#include <svanes/entity.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>

namespace svanes {

struct GameContext;
struct FrameContext;
class Registry;

}

/**
 * What the goose should attempt over a single frame. The game fills this in from
 * the keyboard and mouse, so the goose works in terms of what the player wants to
 * do rather than which keys are down. Rebinding a key, or handing the goose to
 * something other than a player, changes only how this struct gets filled.
 *
 * FIELDS:
 * - move: Walk direction. Only the x component is read: -1 walks left, +1 walks
 *   right, 0 stands still.
 * - dash: Dash direction, as -1 for left, +1 for right, and 0 for no dash.
 *   Ignored while a dash is already running or on cooldown.
 * - jump: Whether the jump input is held. Launches a jump from the ground, and
 *   sustains flight in the air for as long as flight time remains.
 * - fire: Whether the fire input is held. Firing is rate limited internally, so
 *   holding it produces a steady stream rather than one bullet per frame.
 * - aim_point: The world position bullets are fired toward.
 */
struct GooseIntent {
    svanes::Vector2D move;
    float dash = 0.0F;
    bool jump = false;
    bool fire = false;
    svanes::Vector2D aim_point;
};

/**
 * Which animation the goose is currently showing. State is chosen fresh each frame
 * from what the goose is doing, and only decides which texture and SpriteAnimation
 * are bound to the entity.
 *
 * MEMBERS:
 * - Idle: Standing still. Draws a single frame with no animation attached.
 * - Walking: Moving horizontally. Plays the walk cycle.
 * - Flying: Airborne under held jump. Plays the walk cycle at a faster rate.
 */
enum class GooseState : std::uint8_t {
    Idle,
    Walking,
    Flying,
};

/**
 * The player character. Owns one entity and moves it by writing to the engine's
 * Kinematic2D component, letting Gravity and AdvanceKinematics do the integration.
 *
 * The goose can walk, jump, dash along the ground, and fly for a limited time that
 * refills whenever it lands. It fires bullets toward an aim point, and takes
 * knockback from bullets and from touching an enemy.
 */
class Goose final {
public:
    /**
     * Creates the goose entity, loads its textures, and places it in the world.
     *
     * @param context The game context supplying the registry and texture loader.
     * @param x The world x position to spawn at.
     * @param y The world y position to spawn at.
     *
     * @throws std::logic_error if the goose has already been spawned.
     */
    void Spawn(svanes::GameContext& context, float x, float y);

    /**
     * Advances the goose by one frame: resolves collisions against solid geometry,
     * applies the intent to its velocity, fires any bullets that are due, and picks
     * the animation state to match.
     *
     * @param frame The frame context supplying the registry and the frame delta.
     * @param intent What the goose should attempt this frame.
     *
     * @throws std::logic_error if the goose has not been spawned.
     */
    void Update(const svanes::FrameContext& frame, const GooseIntent& intent);

    /**
     * Switches the goose to an animation state, swapping the texture and attaching
     * or removing the SpriteAnimation component. Does nothing if the goose is
     * already in that state, so the animation is not restarted every frame.
     *
     * @param world The registry holding the goose entity.
     * @param next The state to switch to.
     */
    void SetState(svanes::Registry& world, GooseState next);

    /**
     * Throws the goose along a direction and starts the knockback and invincibility
     * timers. Player control is suspended until the knockback timer expires, and
     * further knockback is ignored until the invincibility timer does.
     *
     * @param world The registry holding the goose entity.
     * @param direction The direction to be thrown in. Need not be normalized.
     * @param speed The speed to be thrown at, in world units per second.
     *
     * @throws std::logic_error if the goose has not been spawned.
     * @throws std::invalid_argument if the direction is not finite or is zero length.
     * @throws std::invalid_argument if the speed is not finite or is not positive.
     */
    void ApplyKnockback(svanes::Registry& world, svanes::Vector2D direction, float speed);

    /**
     * @return The goose's entity, for looking up its components or comparing
     * against the target of a bullet hit.
     */
    svanes::Entity GetEntity() const;

private:
    void ResolveCollisions(svanes::Registry& world);

    svanes::Entity entity = 0;
    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle walk_texture{};

    GooseState state = GooseState::Idle;
    float time_in_state = 0.0F;

    // whether the goose is standing on solid geometry, recalculated every frame
    // from the collision normals in ResolveCollisions
    bool grounded = false;

    bool spawned = false;

    float speed = 300.0F;
    float jump_speed = 900.0F;
    float max_fall_speed = 1500.0F;

    float fly_rise_speed = 800.0F;
    float fly_rise_acceleration = 4600.0F;

    // flight is a budget rather than a cooldown: it drains while flying and
    // refills to max_fly_seconds on landing, so the goose cannot hover forever
    float max_fly_seconds = 2.0F;
    float fly_time_remaining = 0.0F;

    float dash_speed = 1400.0F;
    float dash_seconds = 0.15F;
    float dash_cooldown_seconds = 1.0F;

    // counts down while a dash is running, during which horizontal input is ignored
    float dash_timer = 0.0F;

    float dash_cooldown = 0.0F;
    float fire_cooldown = 0.0F;

    // counts down while the goose is being knocked back, during which it ignores
    // player input so a hit cannot be immediately walked off
    float knockback_timer = 0.0F;

    // counts down after a hit, during which further knockback is ignored
    float invincible_timer = 0.0F;
};

#pragma once

#include "game_components.hpp"

#include <svanes/entity.hpp>
#include <svanes/kinematic_system.hpp>
#include <svanes/render/basic_render_types.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/stable_id.hpp>
#include <svanes/vector2d.hpp>

#include <cstdint>
#include <optional>

namespace svanes {

struct FrameContext;
class Registry;
class TextureManager;

}

/**
 * The textures every goose draws with, loaded once and shared between geese.
 *
 * FIELDS:
 * - idle: The single standing frame.
 * - walk: The walk-cycle sprite sheet, also used while flying.
 */
struct GooseTextures {
    svanes::TextureHandle idle{};
    svanes::TextureHandle walk{};
};

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
 * - aim: The direction bullets are fired toward, as an offset from the goose's position.
 */
struct GooseIntent {
    svanes::Vector2D move;
    float dash = 0.0F;
    bool jump = false;
    bool fire = false;
    svanes::Vector2D aim;
};

/**
 * Which parts of an intent a step depended on. Parts not listed here are read
 * on every step, so a different value for them always could have changed it.
 *
 * FIELDS:
 * - aim: Whether the aim was read. The goose only reads it when trying to fire.
 */
struct GooseIntentUse {
    bool aim = false;
};

/**
 * Whether a step that consumed one intent could have turned out differently with another.
 * @param used The intent the step consumed.
 * @param other The intent to compare against.
 * @param use Which parts of the intent the step depended on.
 * @return Whether any part the step depended on differs.
 */
bool ChangesStep(const GooseIntent& used, const GooseIntent& other, GooseIntentUse use);

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
 * Local snapshot of a goose, including the engine components changed by simulation.
 * Texture handles refer to this process's assets and are not network identifiers,
 * so Restore chooses the texture from the state rather than from the sprite.
 *
 * FIELDS:
 * - id: The goose's shared identity.
 * - transform: Position and orientation.
 * - motion: Velocity, acceleration, and motion limits.
 * - timeline: Local clock and its accumulated progress.
 * - sprite: The displayed texture and source rectangle.
 * - animation: Animation progress, or empty for an idle sprite.
 * - state: The chosen animation state.
 * - grounded: Whether the last collision pass found supporting ground.
 * - fly_time_remaining: Remaining flight budget in timeline tics.
 * - dash_timer: Remaining dash duration in timeline tics.
 * - dash_cooldown: Time before another dash, in timeline tics.
 * - fire_cooldown: Time before another shot, in timeline tics.
 * - knockback_timer: Remaining loss of control, in timeline tics.
 * - invincible_timer: Remaining protection from knockback, in timeline tics.
 */
struct GooseSnapshot {
    svanes::StableId id;
    svanes::Transform transform;
    svanes::Kinematic2D motion;
    svanes::Timeline timeline;
    svanes::Sprite sprite;
    std::optional<svanes::SpriteAnimation> animation;
    GooseState state;
    bool grounded;
    svanes::TicCount fly_time_remaining;
    svanes::TicCount dash_timer;
    svanes::TicCount dash_cooldown;
    svanes::TicCount fire_cooldown;
    svanes::TicCount knockback_timer;
    svanes::TicCount invincible_timer;
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
     * Loads the textures a goose draws with.
     * @param assets The texture manager to load through.
     * @return The loaded handles, to be passed to Spawn.
     */
    static GooseTextures LoadTextures(svanes::TextureManager& assets);

    /**
     * Creates the goose entity and places it in the world.
     *
     * @param world The registry the goose entity is created in.
     * @param textures Textures from LoadTextures.
     * @param x The world x position to spawn at.
     * @param y The world y position to spawn at.
     * @param id The goose's shared identity.
     *
     * @throws std::logic_error if the goose has already been spawned.
     */
    void Spawn(svanes::Registry& world, const GooseTextures& textures, float x, float y, svanes::StableId id);

    /**
     * Applies input and advances timers after a physics step. Receives elapsed
     * simulation time explicitly so the same step can be replayed.
     * @param world The registry containing the goose.
     * @param intent The input for this step.
     * @param delta_tics The goose's elapsed local timeline tics.
     * @return Which parts of the intent this step depended on.
     * @throws std::logic_error if the goose has not been spawned.
     */
    GooseIntentUse Advance(svanes::Registry& world, const GooseIntent& intent,
                 svanes::TicCount delta_tics);

    /**
     * Captures the controller and engine state needed to replay movement.
     * @param world The registry containing the goose.
     * @return A local snapshot of this goose.
     * @throws std::logic_error if the goose has not been spawned.
     */
    GooseSnapshot Capture(const svanes::Registry& world) const;

    /**
     * Restores a snapshot onto a goose without spawning another entity. The sprite's
     * texture follows the snapshot's state, and its geometry is left unchanged.
     * @param world The registry containing the goose.
     * @param snapshot A snapshot captured from this goose or received for it.
     * @throws std::logic_error if the goose has not been spawned.
     */
    void Restore(svanes::Registry& world, const GooseSnapshot& snapshot);

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
    // refills to max_fly_tics on landing, so the goose cannot hover forever
    svanes::TicCount max_fly_tics = svanes::SecondsToTics(2.0);
    svanes::TicCount fly_time_remaining = 0;

    float dash_speed = 1400.0F;
    svanes::TicCount dash_tics = svanes::SecondsToTics(0.15);
    svanes::TicCount dash_cooldown_tics = svanes::SecondsToTics(1.0);

    // counts down while a dash is running, during which horizontal input is ignored
    svanes::TicCount dash_timer = 0;

    svanes::TicCount dash_cooldown = 0;
    svanes::TicCount fire_cooldown = 0;

    // counts down while the goose is being knocked back, during which it ignores
    // player input so a hit cannot be immediately walked off
    svanes::TicCount knockback_timer = 0;

    // counts down after a hit, during which further knockback is ignored
    svanes::TicCount invincible_timer = 0;
};

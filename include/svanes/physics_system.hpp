#pragma once

#include <svanes/timeline_system.hpp>
#include <svanes/vector2d.hpp>

#include <functional>
#include <span>

namespace svanes {

class AsyncParallelForDriver;
class InputManager;
class Registry;

// How many source tics make up one simulation step. A source tic is one tic
// from the shared simulation clock, measured in microseconds here. Currently
// this is 10ms of source time.
inline constexpr TicCount DefaultPhysicsStepTics = 10000;

/**
 * Represents how much local time one entity advances in a simulation step.
 *
 * Suppose we advance the shared simulation clock by 10000 source tics. A
 * source tic is one tic from that clock, measured in microseconds here. An
 * entity running at normal speed receives 10000 local tics, while one at double
 * speed receives 20000. Both are updated in the same simulation step. They just
 * have different amounts of local time with which to update their motion.
 *
 * FIELDS:
 * - entity: The unique identifier of the entity whose physics are being
 * updated.
 * - delta_tics: The entity's local elapsed tics for this simulation step.
 * Zero means it does not advance, including not clamping its motion. It can
 * still be a collider or an attractor affecting other entities.
 */
struct PhysicsTimeStep {
    Entity entity;
    TicCount delta_tics;
};

/**
 * Provides the context for a game's post-physics update. It is passed to
 * IGame::PhysicsUpdate after one simulation step has moved the entities and
 * before the next step begins. A rendered frame may generate several
 * PhysicsContext instances, one for each simulation step. or none at all if no
 * complete simulation step has elapsed.
 *
 * FIELDS:
 * - world: The registry containing the entities and their components. The
 * game may inspect or modify it while responding to the completed step.
 * - input: The input sampled for the current rendered frame. Several
 * post-physics updates may receive the same input during a slow frame,
 * as inputs are not sampled more than once per frame.
 * - entity_steps: The entities moved during the completed step and their local
 * time deltas. The span is valid only during the post-physics update. If the
 * game destroys an entity, its entry remains in the span, so check the
 * registry before accessing that entity's components.
 */
struct PhysicsContext {
    Registry &world;
    const InputManager &input;
    std::span<const PhysicsTimeStep> entity_steps;
};

/**
 * Advances physics for one simulation step using the local time deltas
 * previously reported by AdvanceTimelines. Attractor forces are evaluated
 * before kinematic movement, and the game specific post physics callback is
 * called after movement completes.
 *
 * AdvanceTimelines MUST be called before this function.
 *
 * @param world The registry containing all entities and their components.
 * @param gravity The acceleration applied to entities with Gravity, in world
 * units per local tic squared. Point attractors contribute independently.
 * @param driver The driver used to parallelize any parallelizable work.
 * @param after_step The game specified callback invoked after movement
 * completes for the simulation step.
 *
 * @throws std::invalid_argument if the callback is empty, or if kinematics
 * encounters nonfinite gravity or invalid motion limits.
 */
void AdvancePhysics(
    Registry &world, Vector2D gravity, AsyncParallelForDriver &driver,
    const std::function<void(std::span<const PhysicsTimeStep>)> &after_step);

} // namespace svanes

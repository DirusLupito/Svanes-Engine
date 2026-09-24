#pragma once

#include "goose.hpp"
#include "enemy.hpp"
#include "bullets.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/network/network_session.hpp>

#include <span>
#include <vector>

// One gameplay tick is 10 ms, or 10000 of the engine's microsecond timeline tics.
inline constexpr svanes::TicCount GooseStepTics = 10000;

/**
 * Captures the players, enemy, and shots before a simulation tick.
 *
 * FIELDS:
 * - tick: The next input tick to simulate after restoring this snapshot.
 * - geese: Goose snapshots in the simulation's ascending peer order.
 * - enemy: The enemy's position, health, firing timer, and life state.
 * - bullets: Live shots in simulation order, with local owner references.
 * - next_bullet_id: The next shot sequence number, restored before replay.
 */
struct GooseWorldSnapshot {
    std::uint64_t tick;
    std::vector<GooseSnapshot> geese;
    EnemySnapshot enemy;
    std::vector<BulletSnapshot> bullets;
    std::uint64_t next_bullet_id = 1;
};

/**
 * Advances the roster's geese against the game's static arena in fixed steps.
 * Peer order determines spawning, input indexing, and snapshot indexing, so
 * every process applies the same inputs to the same players. A step advances
 * the geese's clocks and engine physics, then applies their controller input
 * and collision response. Render timing and keyboard sampling live outside it.
 * The enemy follows a path derived from that same tick counter, so replaying
 * a step gives it the same destination without exchanging enemy positions.
 *
 * Capture and Restore include controller timers, motion, clocks, and animation
 * progress. Restore recreates bullets and restores enemy life while preserving
 * player and enemy entity identities. Shot ids and ordered collision passes
 * keep replay independent of the registry ids assigned to recreated bullets.
 * The caller controls when to step, including replay from a restored state.
 */
class GooseSimulation final {
public:
    /**
     * Spawns one goose for each peer and an enemy, and takes control of simulation.
     * @param context The world and asset services used to create the geese.
     * @param roster The nonempty roster in ascending, unique peer id order.
     * @throws std::invalid_argument if the roster is empty, unsorted, or contains zero ids.
     * @throws std::logic_error if the simulation has already been initialized.
     */
    void Initialize(svanes::GameContext& context, std::span<const svanes::PeerId> roster);

    /**
     * Advances one fixed combat step using inputs in roster order.
     * @param world The registry containing the geese and static arena.
     * @param gravity The world gravity in units per timeline tic squared.
     * @param inputs One movement and firing intent for each peer, with world-space aim.
     * @throws std::invalid_argument for a wrong input count or invalid movement inputs.
     * @throws std::logic_error if the simulation has not been initialized.
     * @throws std::overflow_error if the tick counter is exhausted.
     */
    void Step(svanes::Registry& world, svanes::Vector2D gravity,
              std::span<const GooseIntent> inputs);

    /**
     * Captures the current tick boundary for later replay.
     * @param world The registry containing the geese.
     * @return A snapshot belonging to this simulation and roster.
     * @throws std::logic_error if the simulation has not been initialized.
     */
    GooseWorldSnapshot Capture(const svanes::Registry& world) const;

    /**
     * Hashes movement and combat state using explicit serialized fields in shot order.
     * Local entity ids, texture handles, and presentation state are excluded.
     * @param snapshot The tick boundary to compare across peers.
     * @return A deterministic diagnostic hash of the recorded gameplay state.
     */
    static std::uint64_t Hash(const GooseWorldSnapshot& snapshot);

    /**
     * Restores the geese, enemy, shots, and tick counter to an earlier boundary.
     * @param world The registry containing the geese.
     * @param snapshot A snapshot captured from this simulation.
     * @throws std::invalid_argument if the snapshot has a different player count.
     * @throws std::logic_error if the simulation has not been initialized.
     */
    void Restore(svanes::Registry& world, const GooseWorldSnapshot& snapshot);

    /**
     * Looks up the entity belonging to a roster member, including the local player.
     * @param peer The player to look up.
     * @return That player's goose entity, for camera tracking or presentation.
     * @throws std::invalid_argument if the peer is outside the roster.
     */
    svanes::Entity PlayerEntity(svanes::PeerId peer) const;

    /** @return The input tick that the next Step() will consume. */
    std::uint64_t Tick() const;

private:
    /**
     * Associates the network player identity with its game controller.
     * FIELDS:
     * - peer: The shared identity of the player supplying this goose's input.
     * - goose: The controller and local entity for that player.
     */
    struct Player {
        svanes::PeerId peer;
        Goose goose;
    };

    std::vector<Player> players;
    Enemy enemy;
    std::uint64_t next_bullet_id = 1;
    svanes::AsyncParallelForDriver physics_driver{1};
    std::uint64_t tick = 0;
};

#pragma once

#include "goose.hpp"
#include "enemy.hpp"
#include "bullets.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/network/network_session.hpp>

#include <span>
#include <vector>

namespace svanes {

struct GameContext;

}

// One gameplay tick is 10 ms, or 10000 of the engine's microsecond timeline tics.
inline constexpr svanes::TicCount GooseStepTics = 10000;

/** World gravity in units per second squared. Positive y is down. */
inline constexpr float GooseGravity = 2000.0F;

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
 * Peer order determines input indexing and snapshot indexing, so every process
 * applies the same inputs to the same players. A step advances the geese's
 * clocks and engine physics, then applies their controller input and collision
 * response. Render timing and keyboard sampling live outside it. The enemy
 * follows a path derived from that same tick counter, so replaying a step gives
 * it the same destination without exchanging enemy positions.
 *
 * Capture and Restore include controller timers, motion, clocks, and animation
 * progress. Restore recreates bullets and restores enemy life while preserving
 * player and enemy entity identities. Shot ids, owner peer ids, and bullet
 * targets ordered by gameplay identity keep simulation independent of the
 * registry ids a process assigns, so a joining process can rebuild the world
 * from Encode's bytes and continue identically.
 * The caller controls when to step, including replay from a restored state.
 */
class GooseSimulation final {
public:
    /**
     * Loads goose textures, spawns the enemy, and takes control of simulation.
     * Players are added separately with AddPlayer.
     * @param context The world and asset services used by the simulation.
     * @throws std::logic_error if the simulation has already been initialized.
     */
    void Initialize(svanes::GameContext& context);

    /**
     * Spawns a goose for a new player at the first unoccupied spawn point, or at
     * a point chosen from the peer id when all are occupied.
     * @param world The registry to spawn in.
     * @param peer The new player's id, greater than every current player's id.
     * @throws std::invalid_argument for a zero id or one not above the current roster.
     * @throws std::logic_error if the simulation has not been initialized.
     */
    void AddPlayer(svanes::Registry& world, svanes::PeerId peer);

    /**
     * Removes a goose at an agreed roster boundary, leaving its existing shots alive.
     * @param world The registry containing the goose.
     * @param peer The departing player's identity.
     * @throws std::invalid_argument if the peer is outside the roster.
     */
    void RemovePlayer(svanes::Registry& world, svanes::PeerId peer);

    /**
     * Advances one fixed combat step using inputs in roster order.
     * @param world The registry containing the geese and static arena.
     * @param gravity The world gravity in units per timeline tic squared.
     * @param inputs One movement and firing intent for each peer, with goose-relative aim.
     * @return Which parts of each peer's intent the step depended on, in roster order.
     * @throws std::invalid_argument for a wrong input count or invalid movement inputs.
     * @throws std::logic_error if the simulation has not been initialized.
     * @throws std::overflow_error if the tick counter is exhausted.
     */
    std::vector<GooseIntentUse> Step(svanes::Registry& world, svanes::Vector2D gravity,
              std::span<const GooseIntent> inputs);

    /**
     * Captures the current tick boundary for later replay.
     * @param world The registry containing the geese.
     * @return A snapshot belonging to this simulation and roster.
     * @throws std::logic_error if the simulation has not been initialized.
     */
    GooseWorldSnapshot Capture(const svanes::Registry& world) const;

    /**
     * Writes every gameplay and animation field of a snapshot in a portable form.
     * Local entity ids and texture handles are excluded.
     * @param writer The message to append to.
     * @param snapshot The tick boundary to encode.
     * @throws std::logic_error for a timeline that is paused, parented, or scaled.
     */
    static void Encode(svanes::MessageWriter& writer, const GooseWorldSnapshot& snapshot);

    /**
     * Reads a snapshot written by Encode, mapping shot owners onto this process's entities.
     * @param reader The message positioned at the encoded snapshot.
     * @return A snapshot that Restore can apply to this simulation.
     * @throws std::invalid_argument for malformed data or a different player count.
     */
    GooseWorldSnapshot Decode(svanes::MessageReader& reader) const;

    /**
     * Hashes the bytes Encode produces, so peers compare exactly what a joiner receives.
     * @param snapshot The tick boundary to compare across peers.
     * @return A deterministic diagnostic hash of the recorded state.
     */
    static std::uint64_t Hash(const GooseWorldSnapshot& snapshot);

    /**
     * Restores the geese, enemy, shots, and tick counter to an earlier boundary.
     * @param world The registry containing the geese.
     * @param snapshot A snapshot captured from this simulation or decoded for it.
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

    /** @return The players' ids in ascending order. */
    std::vector<svanes::PeerId> Roster() const;

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

    /**
     * @param world The registry containing the arena, geese, and enemy.
     * @return Bullet targets: the arena in creation order, geese in peer order, then the enemy.
     */
    std::vector<svanes::Entity> BulletTargets(const svanes::Registry& world) const;

    bool initialized = false;
    GooseTextures textures;
    std::vector<Player> players;
    Enemy enemy;
    svanes::Entity departed_owner = 0;
    std::uint64_t next_bullet_id = 1;
    svanes::AsyncParallelForDriver physics_driver{1};
    std::uint64_t tick = 0;
};

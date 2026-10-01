#pragma once

#include "goose.hpp"
#include "enemy.hpp"
#include "bullets.hpp"

#include <svanes/async/async_parallel_for_driver.hpp>
#include <svanes/game.hpp>
#include <svanes/network/input_sync.hpp>
#include <svanes/network/message_serialization.hpp>

#include <span>
#include <vector>

// One gameplay tick is 10 ms, or 10000 of the engine's microsecond timeline tics.
inline constexpr svanes::TicCount GooseStepTics = 10000;

/** World gravity in units per second squared. Positive y is down. */
inline constexpr float GooseGravity = 2000.0F;

/** The most players a world admits, one per spawn point. */
inline constexpr std::uint32_t GooseMaxPlayers = 8;

/**
 * Captures the players, enemy, and shots before a simulation tick.
 *
 * FIELDS:
 * - tick: The next input tick to simulate after restoring this snapshot.
 * - geese: Goose snapshots in the simulation's ascending peer order.
 * - enemy: The enemy's position, health, firing timer, and life state.
 * - bullets: Live shots in simulation order, with local owner references.
 * - next_stable_id: The next StableId to assign, restored before replay.
 */
struct GooseWorldSnapshot {
    std::uint64_t tick;
    std::vector<GooseSnapshot> geese;
    EnemySnapshot enemy;
    std::vector<BulletSnapshot> bullets;
    std::uint64_t next_stable_id = 1;
};

/**
 * The goose game's synchronized simulation: the roster's geese, the enemy, and
 * bullets against the static arena, advanced in fixed steps. InputSync keeps it
 * identical on every member by feeding every player's input to Step().
 *
 * Peer order determines input indexing and snapshot indexing, so every process
 * applies the same inputs to the same players. A step advances the geese's
 * clocks and engine physics, then applies their controller input and collision
 * response. The enemy follows a path derived from the tick counter, so
 * replaying a step gives it the same destination without exchanging enemy
 * positions.
 *
 * Save and Load cover controller timers, motion, clocks, and animation
 * progress. Load recreates bullets and restores enemy life while preserving
 * player and enemy entity identities. Geese, the enemy, and shots carry
 * StableIds from one shared counter. Shots are ordered by them and remember
 * their owner by them, which keeps simulation independent of the registry ids
 * a process assigns, so a joining process can rebuild the world from saved
 * bytes and continue identically.
 *
 * Local input sampling, including double-tap dashing and goose-relative aim,
 * also lives here. Held controls are read every frame and presses are kept
 * until the next tick takes them.
 */
class GooseSimulation final : public svanes::SyncedSimulation<GooseIntent, GooseIntentUse> {
public:
    /**
     * Builds the arena and spawns the enemy. Players are added separately with AddPlayer.
     * Needs no window, so a headless server can run the same simulation.
     * @param world The registry the simulation lives in.
     * @param gravity World gravity in units per local tic squared.
     * @param textures The textures geese draw with, or empty handles where nothing is drawn.
     */
    GooseSimulation(svanes::Registry& world, svanes::Vector2D gravity, GooseTextures textures);

    /** @return Every gameplay and animation field, without local entity ids or texture handles. */
    svanes::NetworkMessage Save() const override;

    /**
     * Restores the geese, enemy, shots, and tick counter from saved bytes.
     * @param saved Bytes written by Save() with the same players.
     * @throws std::invalid_argument for malformed data or a different player count.
     */
    void Load(std::span<const std::byte> saved) override;

    /**
     * Advances one fixed combat step using inputs in roster order.
     * @param inputs One movement and firing intent for each peer, with goose-relative aim.
     * @return Which parts of each peer's intent the step depended on, in roster order.
     * @throws std::invalid_argument for a wrong input count or invalid movement inputs.
     * @throws std::overflow_error if the tick counter is exhausted.
     */
    std::vector<GooseIntentUse> Step(std::span<const GooseIntent> inputs) override;

    /**
     * Reads held movement, jump, fire, and aim, and remembers direction presses.
     * @param frame The frame's input, camera, and world.
     * @param local The player whose goose aim is relative to.
     */
    void CaptureInput(const svanes::FrameContext& frame, svanes::PeerId local) override;

    /**
     * Consumes remembered presses, turning a second press within the double-tap
     * window into a dash.
     * @return The local intent for the next tick.
     */
    GooseIntent TakeInput() override;

    /**
     * Writes an intent with movement directions encoded as 0, 1, and 2.
     * @param writer The message to append to.
     * @param input The intent to write.
     */
    void EncodeInput(svanes::MessageWriter& writer, const GooseIntent& input) const override;

    /**
     * @param reader The message positioned at an intent written by EncodeInput.
     * @return The intent.
     * @throws std::invalid_argument for an invalid direction or aim.
     */
    GooseIntent DecodeInput(svanes::MessageReader& reader) const override;

    /**
     * Spawns a goose for a new player at the first unoccupied spawn point, or at
     * a point chosen from the peer id when all are occupied.
     * @param peer The new player's id, greater than every current player's id.
     * @throws std::invalid_argument for a zero id or one not above the current roster.
     */
    void AddPlayer(svanes::PeerId peer) override;

    /**
     * Removes a goose, leaving its existing shots alive.
     * @param peer The departing player's identity.
     * @throws std::invalid_argument if the peer is outside the roster.
     */
    void RemovePlayer(svanes::PeerId peer) override;

    /**
     * @param last The player's latest known intent.
     * @return The same held controls and aim, without repeating a dash.
     */
    GooseIntent Predict(const GooseIntent& last) const override;

    /**
     * @param used The intent the step consumed.
     * @param actual The intent that arrived for that step.
     * @param use Which parts of the intent the step depended on.
     * @return Whether any part the step depended on differs.
     */
    bool ChangesStep(const GooseIntent& used, const GooseIntent& actual,
                     const GooseIntentUse& use) const override;

    /**
     * Looks up the entity belonging to a roster member, including the local player.
     * @param peer The player to look up.
     * @return That player's goose entity, for camera tracking or presentation.
     * @throws std::invalid_argument if the peer is outside the roster.
     */
    svanes::Entity PlayerEntity(svanes::PeerId peer) const;

    /**
     * Stops or resumes reading the local devices. While stopped, CaptureInput
     * reports neutral held controls and remembers no presses.
     * @param enabled Whether local input controls the goose.
     */
    void SetControlsEnabled(bool enabled);

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
     * Local device input waiting for the next tick.
     * FIELDS:
     * - move: The latest held horizontal direction.
     * - jump: Whether jump is held.
     * - fire: Whether the mouse button is held.
     * - aim: The mouse's world position relative to the local goose.
     * - left_pressed: A left press observed since the last taken input.
     * - right_pressed: A right press observed since the last taken input.
     * - left_tap_ticks: Remaining ticks in the left double-tap window.
     * - right_tap_ticks: Remaining ticks in the right double-tap window.
     */
    struct LocalControls {
        float move = 0.0F;
        bool jump = false;
        bool fire = false;
        svanes::Vector2D aim{};
        bool left_pressed = false;
        bool right_pressed = false;
        std::uint32_t left_tap_ticks = 0;
        std::uint32_t right_tap_ticks = 0;
    };

    /** @return The world at the current tick. */
    GooseWorldSnapshot Capture() const;

    /**
     * Writes every gameplay and animation field of a snapshot in a portable form.
     * @param writer The message to append to.
     * @param snapshot The tick boundary to encode.
     */
    static void Encode(svanes::MessageWriter& writer, const GooseWorldSnapshot& snapshot);

    /**
     * Reads a snapshot written by Encode. Shot owners are found by StableId during Restore.
     * @param reader The message positioned at the encoded snapshot.
     * @return A snapshot that Restore can apply to this simulation.
     * @throws std::invalid_argument for malformed data or a different player count.
     */
    GooseWorldSnapshot Decode(svanes::MessageReader& reader) const;

    /**
     * Restores the geese, enemy, shots, and tick counter to an earlier boundary.
     * @param snapshot A snapshot captured from this simulation or decoded for it.
     * @throws std::invalid_argument if the snapshot has a different player count.
     */
    void Restore(const GooseWorldSnapshot& snapshot);

    /**
     * @param world The registry containing the arena, geese, and enemy.
     * @return Bullet targets: the arena in creation order, geese in peer order, then the enemy.
     */
    std::vector<svanes::Entity> BulletTargets(const svanes::Registry& world) const;

    /**
     * @return A StableId no entity has had before, shared by geese, the enemy, and shots.
     * @throws std::overflow_error if the counter is exhausted.
     */
    svanes::StableId NextStableId();

    svanes::Registry& world;
    svanes::Vector2D gravity;
    GooseTextures textures;
    std::vector<Player> players;
    Enemy enemy;
    svanes::Entity departed_owner = 0;
    std::uint64_t next_stable_id = 1;
    svanes::AsyncParallelForDriver physics_driver{1};
    std::uint64_t tick = 0;
    LocalControls controls;
    bool controls_enabled = true;
};

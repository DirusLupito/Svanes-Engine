#pragma once

#include "goose_network.hpp"
#include "goose_simulation.hpp"

#include <map>
#include <optional>
#include <string>

/**
 * Runs the movement simulation from tick-numbered player inputs.
 * Local input is recorded and sent before advancing its tick. Missing remote
 * input repeats that player's last known held controls, with no repeated dash
 * press. When a received input differs from the input used, the controller
 * restores the earliest affected snapshot and replays to the current tick.
 *
 * Prediction is limited to 30 ticks beyond confirmed input. History retains
 * 256 ticks, and each rendered frame advances at most eight new ticks. Those
 * bounds keep waiting peers and slow frames from creating unlimited work.
 * Network delivery continues while simulation waits. Input sampling and
 * double-tap detection belong to the local player and are not replayed.
 * A window that gets ahead of received peer progress temporarily slows its
 * pacing, keeping the usual prediction distance below the hard rollback limit.
 * Every 100 ticks, peers compare hashes of the same confirmed world boundary.
 * A disagreement stops simulation with the peer and tick in the status message.
 */
class GooseRollback final {
public:
    /**
     * Connects an initialized simulation to its fixed-roster network session.
     * @param simulation The movement simulation whose tick begins at zero.
     * @param network The session with the same player ordering.
     * @throws std::invalid_argument if the local peer is absent or simulation has advanced.
     */
    GooseRollback(GooseSimulation& simulation, GooseNetwork& network);

    /**
     * Receives inputs, corrects predictions, and advances due simulation ticks.
     * Call after GooseNetwork::Update() on every rendered frame.
     * @param frame The world, local input, gravity, and elapsed real time.
     * @throws std::invalid_argument for malformed or unexpected game messages.
     */
    void Update(const svanes::FrameContext& frame);

    /** @return A stable status message for the console or game display. */
    std::string Status() const;

    /** @return The first tick for which some player's actual input is missing. */
    std::uint64_t ConfirmedTick() const;

    /** @return How many corrections have restored and replayed simulation. */
    std::uint64_t RollbackCount() const;

    /** @return Tick progress per peer and the depth of the last rollback correction. */
    std::string Diagnostics() const;

private:
    /**
     * Retains one player's actual inputs and a prediction seed before history.
     * FIELDS:
     * - peer: The player supplying these inputs.
     * - actual: Received or locally generated inputs indexed by gameplay tick.
     * - preceding: The latest actual input before the retained history.
     * - next_input_tick: The first missing tick in this player's continuous input stream.
     * - latest_input_tick: One past the highest received input tick, even across gaps.
     */
    struct PlayerInputs {
        svanes::PeerId peer;
        std::map<std::uint64_t, GooseIntent> actual;
        GooseIntent preceding{};
        std::uint64_t next_input_tick = 0;
        std::uint64_t latest_input_tick = 0;
    };

    /**
     * Records one simulated tick before it is confirmed by all peers.
     * FIELDS:
     * - before: The world at the boundary before applying this tick's inputs.
     * - used: The actual or predicted inputs that were applied, in peer order.
     */
    struct TickRecord {
        GooseWorldSnapshot before;
        std::vector<GooseIntent> used;
    };

    /**
     * Holds local device input until a simulation tick can consume it.
     * FIELDS:
     * - move: The latest held horizontal direction.
     * - jump: Whether jump is held.
     * - left_pressed: A left press observed since the last consumed input tick.
     * - right_pressed: A right press observed since the last consumed input tick.
     * - left_tap_ticks: Remaining ticks in the left double-tap window.
     * - right_tap_ticks: Remaining ticks in the right double-tap window.
     */
    struct LocalControls {
        float move = 0.0F;
        bool jump = false;
        bool left_pressed = false;
        bool right_pressed = false;
        std::uint32_t left_tap_ticks = 0;
        std::uint32_t right_tap_ticks = 0;
    };

    /**
     * Validates and records incoming inputs and state hashes without advancing simulation.
     * Sets the earliest correction tick when a prediction differs.
     */
    void ReceiveMessages();

    /**
     * Stores the hashes for one shared simulation boundary until all peers agree.
     * FIELDS:
     * - local: The hash computed after all earlier inputs are confirmed and replayed.
     * - remote: Received hashes indexed by player roster position.
     * - sent: Whether delivery has accepted the local hash for every remote peer.
     */
    struct StateCheckpoint {
        std::optional<std::uint64_t> local;
        std::map<std::size_t, std::uint64_t> remote;
        bool sent = false;
    };

    /**
     * Records a peer's hash for a regularly spaced simulation boundary.
     * @param message The sender and serialized boundary tick and hash.
     */
    void ReceiveStateHash(const svanes::SessionMessage& message);

    /**
     * Hashes confirmed snapshots, sends pending hashes, and compares peer results.
     * Call only after correcting predictions for all received inputs.
     * @param frame The current world used when the checkpoint is the current tick.
     */
    void CheckStateHashes(const svanes::FrameContext& frame);

    /**
     * Records an actual input and advances that player's continuous-input boundary.
     * @param index The player's roster index.
     * @param tick The tick to which the input belongs.
     * @param input The player's actual input.
     */
    void RecordInput(std::size_t index, std::uint64_t tick, const GooseIntent& input);

    /**
     * Chooses an actual input or predicts held controls from an earlier actual input.
     * @param index The player's roster index.
     * @param tick The tick being simulated.
     * @return The input to apply, with dash cleared when predicting.
     */
    GooseIntent InputFor(std::size_t index, std::uint64_t tick) const;

    /**
     * Consumes latched local presses and advances double-tap timers by one tick.
     * @return The local movement input for the next tick.
     */
    GooseIntent TakeLocalInput();

    /**
     * Saves the current boundary and advances one tick using actual or predicted inputs.
     * @param frame The world and gravity used for the step.
     */
    void AdvanceTick(const svanes::FrameContext& frame);

    /** Removes old snapshots and inputs while retaining a seed for prediction. */
    void PruneHistory();

    /**
     * Chooses the real-time interval before another fixed simulation step.
     * @return Twice the normal interval when ahead of remote progress, otherwise normal.
     */
    svanes::TicCount PacingStepTics() const;

    GooseSimulation& simulation;
    GooseNetwork& network;
    std::vector<PlayerInputs> players;
    std::size_t local_index = 0;
    std::map<std::uint64_t, TickRecord> history;
    std::map<std::uint64_t, StateCheckpoint> checkpoints;
    std::uint64_t hashed_tick = 0;
    std::uint64_t verified_tick = 0;
    std::optional<std::uint64_t> correction_tick;
    LocalControls controls;
    svanes::TicCount pending_tics = 0;
    std::uint64_t rollback_count = 0;
    std::uint64_t last_rollback_depth = 0;
    bool started = false;
    std::string failure;
    std::string waiting;
};

#pragma once

#include "goose_network.hpp"
#include "goose_simulation.hpp"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/** The most players a world admits. */
inline constexpr std::size_t GooseMaxPlayers = 8;

/**
 * Runs the gameplay simulation from tick-numbered player inputs.
 * Local input is recorded and sent before advancing its tick. Missing remote
 * input repeats that player's last known held controls and aim, with no repeated
 * dash press. When a received input differs from the input used, the controller
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
 *
 * Players join and leave at coordinated roster boundaries. Any member can be
 * contacted by a joining process. It queues the request, pauses the world with
 * everyone else, and at the agreed boundary every member assigns the same new
 * ids and spawns the new geese. The contacted member then sends the joiner its
 * id, the members' addresses, and the world snapshot. Existing members resume
 * immediately, predicting the joiner's input until its first inputs arrive.
 */
class GooseRollback final {
public:
    /**
     * Runs a world whose simulation already holds the network's roster.
     * @param simulation The gameplay simulation with one goose per member.
     * @param network The session with the same player ordering.
     * @throws std::invalid_argument if the simulation and network rosters differ.
     */
    GooseRollback(GooseSimulation& simulation, GooseNetwork& network);

    /**
     * Rebuilds a joined world from a sponsor's snapshot and verifies it.
     * @param simulation An initialized simulation with no players.
     * @param network The joined network, whose roster the snapshot must match.
     * @param world The registry the players are spawned in.
     * @param snapshot The complete snapshot from GooseNetwork::TakeSnapshot().
     * @throws std::invalid_argument for malformed data.
     * @throws std::runtime_error if the rebuilt world or roster differs from the sponsor's.
     */
    GooseRollback(GooseSimulation& simulation, GooseNetwork& network, svanes::Registry& world,
                  const svanes::NetworkMessage& snapshot);

    /**
     * @return A hash of the fixed rules that joining processes must share.
     */
    static std::uint64_t RulesHash();

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

    /** Requests departure at the next coordinated roster boundary. */
    void RequestLeave();

    /** @return Whether the local goose has left the simulation. */
    bool HasDeparted() const;

    /** @return Whether the window can close after final deliveries, or after a failed session. */
    bool CanClose() const;


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
     * - fire: Whether the mouse button is held.
     * - aim_point: The mouse position converted to world space before recording input.
     * - left_pressed: A left press observed since the last consumed input tick.
     * - right_pressed: A right press observed since the last consumed input tick.
     * - left_tap_ticks: Remaining ticks in the left double-tap window.
     * - right_tap_ticks: Remaining ticks in the right double-tap window.
     */
    struct LocalControls {
        float move = 0.0F;
        bool jump = false;
        bool fire = false;
        svanes::Vector2D aim_point{};
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
     * @return The local movement and firing input for the next tick.
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


    /**
     * Records a peer's frozen input boundary and the roster changes it brings.
     * FIELDS:
     * - tick: The first tick for which that peer has not generated input.
     * - leaving: Whether that peer is departing in this revision.
     * - joining: Addresses of the processes that peer is admitting, in request order.
     */
    struct StopRecord {
        std::uint64_t tick;
        bool leaving;
        std::vector<svanes::UdpAddress> joining;
        bool operator==(const StopRecord&) const = default;
    };

    /**
     * A process asking this member to admit it.
     * FIELDS:
     * - connection: The pipe connection its requests arrive on.
     * - address: The host and port it listens on.
     */
    struct JoinCandidate {
        svanes::ConnectionId connection;
        svanes::UdpAddress address;
    };

    /**
     * An assignment kept so a lost reply can be repeated.
     * FIELDS:
     * - payload: The JoinAssigned message.
     * - sent_at: When the joiner was admitted.
     */
    struct SentAssignment {
        svanes::NetworkMessage payload;
        std::chrono::steady_clock::time_point sent_at;
    };

    /**
     * Confirms the shared world and roster change at a pause boundary.
     * FIELDS:
     * - tick: The agreed boundary after all final inputs.
     * - world_hash: The confirmed world before removing players.
     * - roster_hash: The ordered stop records, roster revision, and next peer id.
     */
    struct PreparedRecord {
        std::uint64_t tick;
        std::uint64_t world_hash;
        std::uint64_t roster_hash;
        bool operator==(const PreparedRecord&) const = default;
    };

    /**
     * Collects a roster change without choosing an authoritative peer.
     * FIELDS:
     * - local_stop: The frozen local boundary, departure choice, and admitted addresses.
     * - sponsored: The processes this member is admitting in this revision.
     * - stops: Each peer's announced boundary, keyed by peer id.
     * - prepared: Each peer's agreement on the final world and departure set.
     * - stop_sent: Whether transport accepted the local stop announcement.
     * - started_at: Real-time start used to report a stalled change.
     */
    struct RosterPause {
        StopRecord local_stop;
        std::vector<JoinCandidate> sponsored;
        std::map<std::uint32_t, StopRecord> stops;
        std::map<std::uint32_t, PreparedRecord> prepared;
        bool stop_sent = false;
        std::chrono::steady_clock::time_point started_at;
    };

    /** Freezes local input generation and records the departure choice and queued joins. */
    void BeginRosterPause();

    /** Answers join requests and queues acceptable ones for the next roster change. */
    void HandleJoinRequests();

    /**
     * Refuses a join request.
     * @param connection The requesting process.
     * @param reason The explanation shown to that player.
     */
    void RejectJoin(svanes::ConnectionId connection, std::string_view reason);

    /**
     * Encodes the current boundary for a joiner: roster, next id, world, and hash.
     * @param world The registry holding the world.
     * @return The complete snapshot message.
     */
    svanes::NetworkMessage EncodeJoinSnapshot(const svanes::Registry& world) const;

    /**
     * Rebuilds per-player input tracking for the network's roster at the current tick,
     * discarding history, corrections, and pending hash checkpoints.
     */
    void ResetPlayers();

    /**
     * Records a stop or prepared message, allowing either to arrive first.
     * @param message The current revision's roster-control message.
     */
    void ReceiveRosterMessage(const svanes::SessionMessage& message);

    /**
     * Catches up to the shared boundary, verifies agreement, and adopts departures.
     * @param frame The world and physics context used for catch-up steps.
     */
    void UpdateRosterPause(const svanes::FrameContext& frame);

    std::optional<RosterPause> roster_pause;
    std::vector<JoinCandidate> join_requests;
    std::map<std::uint32_t, SentAssignment> assignments;
    std::uint32_t next_peer_id = 2;
    bool leave_requested = false;
    bool departed = false;
    bool force_close = false;

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

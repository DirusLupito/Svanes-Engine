#pragma once

#include <svanes/game.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/network/peer_group.hpp>
#include <svanes/timeline_system.hpp>
#include <svanes/utility/hash.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace svanes {

/**
 * Message types InputSync sends, in the range reserved for the engine.
 * - Input: One member's input for one tick.
 * - StateHash: A member's world hash at a confirmed tick.
 */
enum class InputSyncMessageType : MessageType {
    Input = FirstEngineMessageType + 0x10,
    StateHash
};

/**
 * What a game implements to be kept in sync by InputSync. Every member runs
 * the same simulation from the same inputs, so only inputs cross the network.
 * The simulation must be deterministic: the same world and inputs must
 * produce the same world on every platform.
 *
 * A default-constructed Input means nothing pressed. InputSync uses it for
 * ticks a player could not supply, such as while a roster change finishes.
 *
 * @tparam Input One player's controls for one tick.
 * @tparam Use Which parts of an Input a tick read, reported by Step().
 */
template <typename Input, typename Use> class SyncedSimulation {
public:
    virtual ~SyncedSimulation() = default;

    /**
     * @return The whole world as bytes. Rollback history, hash checks, and
     * joiners all use these bytes.
     */
    virtual NetworkMessage Save() const = 0;

    /**
     * Replaces the world with one written by Save(), with the same players.
     * @param world The saved bytes.
     */
    virtual void Load(std::span<const std::byte> world) = 0;

    /**
     * Advances the world by one tick.
     * @param inputs One input per player, in ascending peer order.
     * @return Which parts of each input the tick read, in the same order.
     */
    virtual std::vector<Use> Step(std::span<const Input> inputs) = 0;

    /**
     * Reads the local devices once per rendered frame, remembering presses
     * until TakeInput() consumes them.
     * @param frame The frame's input, camera, and world.
     * @param local The player this process controls.
     */
    virtual void CaptureInput(const FrameContext &frame, PeerId local) = 0;

    /**
     * Called once for each new tick, never during replay.
     * @return The local player's input for the next tick.
     */
    virtual Input TakeInput() = 0;

    /**
     * @param writer The message to append to.
     * @param input The input to write.
     */
    virtual void EncodeInput(MessageWriter &writer,
                             const Input &input) const = 0;

    /**
     * @param reader The message positioned at an input written by
     * EncodeInput().
     * @return The input.
     * @throws std::invalid_argument for malformed or invalid data.
     */
    virtual Input DecodeInput(MessageReader &reader) const = 0;

    /**
     * Adds a player. Players are always added in ascending peer order.
     * @param peer The new player's id.
     */
    virtual void AddPlayer(PeerId peer) = 0;

    /**
     * Removes a player.
     * @param peer The departing player's id.
     */
    virtual void RemovePlayer(PeerId peer) = 0;

    /**
     * Guesses a missing input from the player's last known one.
     * @param last The player's latest input before the missing tick.
     * @return The guess. The default repeats the last input.
     */
    virtual Input Predict(const Input &last) const { return last; }

    /**
     * Whether a tick that used one input could have turned out differently
     * with another. Only such a difference costs a rollback.
     * @param used The input the tick used.
     * @param actual The input that arrived for that tick.
     * @param use Which parts of the input the tick read.
     * @return Whether they could differ in effect. The default compares the
     * encoded inputs and ignores the use report.
     */
    virtual bool ChangesStep(const Input &used, const Input &actual,
                             const Use &use) const {
        static_cast<void>(use);
        MessageWriter used_writer;
        MessageWriter actual_writer;
        EncodeInput(used_writer, used);
        EncodeInput(actual_writer, actual);
        return used_writer.Finish().bytes != actual_writer.Finish().bytes;
    }
};

/**
 * How InputSync paces and bounds its work. Every member must use the same
 * values, so a game includes them in its rules hash.
 * FIELDS:
 * - step_tics: Simulation time per tick, in timeline tics.
 * - prediction_ticks: How far past the last confirmed tick a member may
 *   simulate by guessing inputs. Zero is lockstep: no guessing, no rollback.
 * - pacing_lead_ticks: How far ahead of a peer before this member slows down.
 * - history_ticks: How many past ticks are kept for rollback. Must exceed
 *   prediction_ticks.
 * - hash_interval_ticks: How often members compare world hashes.
 * - max_unchecked_ticks: How far past the last matching hash a member may
 *   simulate. At least hash_interval_ticks.
 * - steps_per_frame: The most new ticks one rendered frame simulates.
 * - max_backlog_steps: The most ticks of real time kept to catch up on.
 */
struct SyncSettings {
    TicCount step_tics;
    std::uint64_t prediction_ticks = 30;
    std::uint64_t pacing_lead_ticks = 3;
    std::uint64_t history_ticks = 256;
    std::uint64_t hash_interval_ticks = 100;
    std::uint64_t max_unchecked_ticks = 200;
    std::uint32_t steps_per_frame = 8;
    std::uint32_t max_backlog_steps = 25;
};

/**
 * Keeps a SyncedSimulation identical on every member by exchanging inputs.
 *
 * Each tick, the local input is sent to every member before the tick runs.
 * A missing remote input is predicted. When the real input arrives and could
 * have changed a tick that used a prediction, the world is restored to that
 * tick and replayed. Prediction reaches at most prediction_ticks past
 * confirmed input, and a member that gets ahead of a peer slows down.
 * Periodically, members compare hashes of the same confirmed tick, and a
 * disagreement stops the simulation with a status message.
 *
 * InputSync is the PeerGroup's RosterParticipant: it adds and removes
 * players at agreed ticks, loads the world when joining, and gives an
 * unresponsive player neutral input from the agreed cutoff.
 *
 * @tparam Input One player's controls for one tick.
 * @tparam Use Which parts of an Input a tick read.
 */
template <typename Input, typename Use>
class InputSync final : public RosterParticipant {
public:
    /**
     * Attaches to the peer group. A world this process started gets its
     * players at once. A world this process is joining is loaded when it
     * arrives.
     * @param peers The members of the world.
     * @param simulation The game's simulation, with no players yet.
     * @param settings Pacing and limits every member shares.
     * @throws std::invalid_argument for inconsistent settings.
     */
    InputSync(PeerGroup &peers, SyncedSimulation<Input, Use> &simulation,
              SyncSettings settings);

    /**
     * Updates the peer group, then receives inputs, corrects predictions,
     * and runs the ticks that are due. Call once per rendered frame instead
     * of PeerGroup::Update().
     * @param frame The frame's input and elapsed real time.
     * @throws std::invalid_argument for malformed or unexpected messages.
     */
    void Update(const FrameContext &frame);

    /** @return A short status for the game's display or console. */
    std::string Status() const;

    /** @return The next tick to simulate. */
    std::uint64_t Tick() const;

    /** @return The first tick for which some player's input is missing. */
    std::uint64_t ConfirmedTick() const;

    /** @return Tick progress per peer and the latest rollback depth. */
    std::string Diagnostics() const;

    /** Leaves at the next roster change, or closes at once after a failure. */
    void RequestLeave();

    /** @return Whether the other members agreed to this process leaving. */
    bool HasDeparted() const;

    /** @return Whether the process can close. */
    bool CanClose() const;

    /**
     * Stops local input and discards pending real time.
     * @return The current tick.
     */
    std::uint64_t Freeze() override;

    /**
     * @param peer Another player.
     * @return The first tick missing that player's input.
     * @throws std::invalid_argument if the peer is not a player.
     */
    std::uint64_t ProgressOf(PeerId peer) override;

    /**
     * Replaces an unresponsive player's inputs from the cutoff with neutral
     * input up to the boundary, correcting ticks that used other inputs.
     * @param peer The player being removed.
     * @param cutoff The first tick whose input is discarded.
     * @param boundary The agreed tick.
     * @throws std::invalid_argument if the peer is not a player.
     */
    void Abandon(PeerId peer, std::uint64_t cutoff,
                 std::uint64_t boundary) override;

    /**
     * Receives inputs, corrects predictions, and advances with neutral local
     * input toward the boundary.
     * @param boundary The agreed tick.
     * @return Whether the simulation is at the boundary with every input
     * through it confirmed.
     */
    bool SettleAt(std::uint64_t boundary) override;

    /** @return The tick followed by the simulation's saved world. */
    NetworkMessage SaveWorld() override;

    /**
     * Removes departing players, adds joining ones, and restarts input
     * tracking.
     * @param departing The members that left.
     * @param joining The members that joined, in ascending order.
     */
    void ChangeRoster(std::span<const PeerId> departing,
                      std::span<const PeerId> joining) override;

    /**
     * Adds the roster's players and restores the received world.
     * @param roster Every member, in ascending order.
     * @param world Bytes written by SaveWorld().
     */
    void LoadWorld(std::span<const PeerId> roster,
                   std::span<const std::byte> world) override;

private:
    /**
     * One player's actual inputs and a prediction seed before history.
     * FIELDS:
     * - peer: The player supplying these inputs.
     * - actual: Received or locally taken inputs indexed by tick.
     * - preceding: The latest actual input before the retained history.
     * - next_input_tick: The first missing tick in this player's inputs.
     * - latest_input_tick: One past the highest received input tick.
     */
    struct PlayerInputs {
        PeerId peer;
        std::map<std::uint64_t, Input> actual;
        Input preceding{};
        std::uint64_t next_input_tick = 0;
        std::uint64_t latest_input_tick = 0;
    };

    /**
     * One simulated tick, kept until it is outside history.
     * FIELDS:
     * - before: The saved world before this tick's inputs.
     * - used: The actual or predicted inputs applied, in peer order.
     * - use: Which parts of each input the tick read, in peer order.
     */
    struct TickRecord {
        NetworkMessage before;
        std::vector<Input> used;
        std::vector<Use> use;
    };

    /**
     * The hashes for one tick until every member agrees.
     * FIELDS:
     * - local: This process's hash, once the tick is confirmed.
     * - remote: Received hashes indexed by player position.
     * - sent: Whether the session accepted the local hash.
     */
    struct StateCheckpoint {
        std::optional<std::uint64_t> local;
        std::map<std::size_t, std::uint64_t> remote;
        bool sent = false;
    };

    /**
     * Rebuilds per-player input tracking for the current roster at the
     * current tick, discarding history, corrections, and checkpoints.
     * @throws std::logic_error if the local peer is not in the roster.
     */
    void ResetPlayers();

    /**
     * @param peer A player's id.
     * @return That player's position in the roster.
     * @throws std::invalid_argument if the peer is not a player.
     */
    std::size_t IndexOf(PeerId peer) const;

    /** Records incoming inputs and hashes without advancing the world. */
    void ReceiveMessages();

    /**
     * Records a peer's hash for a checkpoint tick.
     * @param message The sender and serialized tick and hash.
     */
    void ReceiveStateHash(const SessionMessage &message);

    /** Hashes confirmed checkpoints, sends them, and compares peer hashes. */
    void CheckStateHashes();

    /**
     * Records an actual input and marks a correction if it changes a tick.
     * @param index The player's roster position.
     * @param tick The tick the input belongs to.
     * @param input The player's actual input.
     */
    void RecordInput(std::size_t index, std::uint64_t tick,
                     const Input &input);

    /**
     * @param index The player's roster position.
     * @param tick The tick being simulated.
     * @return The actual input, or a prediction from the last known one.
     */
    Input InputFor(std::size_t index, std::uint64_t tick) const;

    /**
     * @param tick The tick the input belongs to.
     * @param input The input.
     * @return The Input message payload.
     */
    NetworkMessage EncodeTickInput(std::uint64_t tick,
                                   const Input &input) const;

    /**
     * @param input An input.
     * @return Its encoded bytes, for comparing inputs.
     */
    std::vector<std::byte> EncodedBytes(const Input &input) const;

    /**
     * Replaces the world and tick with saved ones.
     * @param saved Bytes written by SaveWorld().
     */
    void RestoreWorld(std::span<const std::byte> saved);

    /** Restores the earliest mispredicted tick and replays to the present. */
    void ApplyCorrection();

    /** Saves the world and advances one tick with actual or predicted inputs. */
    void AdvanceTick();

    /** Removes ticks and inputs older than the history limit. */
    void PruneHistory();

    /**
     * @return Whether one more tick stays within the prediction limit.
     */
    bool CanPredict() const;

    /**
     * @return Twice the step when ahead of a peer, otherwise the step.
     */
    TicCount PacingStepTics() const;

    PeerGroup &peers;
    SyncedSimulation<Input, Use> &simulation;
    SyncSettings settings;
    std::uint64_t tick = 0;
    std::vector<PlayerInputs> players;
    std::size_t local_index = 0;
    std::map<std::uint64_t, TickRecord> history;
    std::map<std::uint64_t, StateCheckpoint> checkpoints;
    std::uint64_t hashed_tick = 0;
    std::uint64_t verified_tick = 0;
    std::optional<std::uint64_t> correction_tick;
    std::optional<Input> unsent_input;
    TicCount pending_tics = 0;
    std::uint64_t rollback_count = 0;
    std::uint64_t last_rollback_depth = 0;
    bool started = false;
    bool force_close = false;
    std::string failure;
    std::string waiting;
};

template <typename Input, typename Use>
InputSync<Input, Use>::InputSync(PeerGroup &peers,
                                 SyncedSimulation<Input, Use> &simulation,
                                 SyncSettings settings)
    : peers(peers), simulation(simulation), settings(settings) {
    if (settings.step_tics == 0 || settings.hash_interval_ticks == 0 ||
        settings.steps_per_frame == 0 || settings.max_backlog_steps == 0 ||
        settings.history_ticks <= settings.prediction_ticks ||
        settings.max_unchecked_ticks < settings.hash_interval_ticks) {
        throw std::invalid_argument("InputSync settings are inconsistent.");
    }
    peers.Attach(*this);
    if (!peers.IsRunning()) {
        return;
    }
    for (const auto peer : peers.Peers()) {
        simulation.AddPlayer(peer);
    }
    ResetPlayers();
}

template <typename Input, typename Use>
void InputSync<Input, Use>::ResetPlayers() {
    players.clear();
    for (const auto peer : peers.Peers()) {
        players.push_back({peer, {}, {}, tick, tick});
    }
    const auto local = std::find(peers.Peers().begin(), peers.Peers().end(),
                                 peers.LocalPeer());
    if (local == peers.Peers().end()) {
        throw std::logic_error("InputSync local peer is absent from the "
                               "roster.");
    }
    local_index = static_cast<std::size_t>(local - peers.Peers().begin());
    history.clear();
    checkpoints.clear();
    correction_tick.reset();
    hashed_tick = tick - tick % settings.hash_interval_ticks;
    verified_tick = hashed_tick;
}

template <typename Input, typename Use>
std::size_t InputSync<Input, Use>::IndexOf(PeerId peer) const {
    for (std::size_t index = 0; index < players.size(); ++index) {
        if (players[index].peer == peer) {
            return index;
        }
    }
    throw std::invalid_argument("InputSync: peer is not a player.");
}

template <typename Input, typename Use>
void InputSync<Input, Use>::Update(const FrameContext &frame) {
    if (!failure.empty()) {
        return;
    }
    peers.Update();
    if (!peers.IsRunning() || peers.HasFailed() || peers.IsChangingRoster() ||
        !failure.empty()) {
        return;
    }
    ReceiveMessages();
    if (!failure.empty()) {
        return;
    }
    const bool first_frame = !started;
    started = true;
    ApplyCorrection();
    if (!failure.empty()) {
        return;
    }
    CheckStateHashes();
    if (!failure.empty()) {
        return;
    }

    simulation.CaptureInput(frame, peers.LocalPeer());
    const TicCount max_backlog = settings.max_backlog_steps * settings.step_tics;
    if (!first_frame) {
        pending_tics += std::min(frame.real_delta_tics, max_backlog - pending_tics);
    }
    for (std::uint32_t step = 0; step < settings.steps_per_frame; ++step) {
        const auto pacing_tics = PacingStepTics();
        if (pending_tics < pacing_tics) {
            break;
        }
        if (tick - verified_tick >= settings.max_unchecked_ticks) {
            waiting = "Waiting for peer state hashes.";
            pending_tics = std::min(pending_tics, settings.step_tics);
            break;
        }
        if (!CanPredict()) {
            waiting = "Waiting for remote inputs.";
            pending_tics = std::min(pending_tics, settings.step_tics);
            break;
        }
        if (!unsent_input) {
            unsent_input = simulation.TakeInput();
        }
        if (!peers.Broadcast(static_cast<MessageType>(InputSyncMessageType::Input),
                             EncodeTickInput(tick, *unsent_input))) {
            waiting = "Waiting for outgoing message capacity.";
            pending_tics = std::min(pending_tics, settings.step_tics);
            break;
        }
        RecordInput(local_index, tick, *unsent_input);
        unsent_input.reset();
        AdvanceTick();
        pending_tics -= pacing_tics;
        waiting.clear();
    }
    CheckStateHashes();
    PruneHistory();
}

template <typename Input, typename Use>
void InputSync<Input, Use>::ReceiveMessages() {
    SessionMessage message;
    while (peers.Receive(message)) {
        if (message.type ==
            static_cast<MessageType>(InputSyncMessageType::StateHash)) {
            ReceiveStateHash(message);
            if (!failure.empty()) {
                return;
            }
            continue;
        }
        if (message.type !=
            static_cast<MessageType>(InputSyncMessageType::Input)) {
            throw std::invalid_argument(
                "InputSync received an unexpected message type.");
        }
        MessageReader reader(message.payload);
        const auto input_tick = reader.ReadUint64();
        const auto input = simulation.DecodeInput(reader);
        if (reader.Remaining() != 0 ||
            input_tick == std::numeric_limits<std::uint64_t>::max()) {
            throw std::invalid_argument("Input message has an invalid tick or "
                                        "length.");
        }
        if (message.sender == peers.LocalPeer()) {
            throw std::invalid_argument("Input message has an invalid sender.");
        }
        const auto index = IndexOf(message.sender);
        if (input_tick > tick && input_tick - tick > settings.history_ticks) {
            failure = "Peer " + std::to_string(message.sender.value) +
                      " sent input beyond the history limit.";
            return;
        }
        if (input_tick < tick && tick - input_tick > settings.history_ticks) {
            if (input_tick >= players[index].next_input_tick) {
                failure = "A missing input is older than retained history.";
                return;
            }
            continue;
        }
        RecordInput(index, input_tick, input);
        if (!failure.empty()) {
            return;
        }
    }
}

template <typename Input, typename Use>
void InputSync<Input, Use>::ReceiveStateHash(const SessionMessage &message) {
    MessageReader reader(message.payload);
    const auto hash_tick = reader.ReadUint64();
    const auto hash = reader.ReadUint64();
    if (reader.Remaining() != 0 || hash_tick == 0 ||
        hash_tick % settings.hash_interval_ticks != 0 ||
        message.sender == peers.LocalPeer()) {
        throw std::invalid_argument("State hash has an invalid sender, tick, "
                                    "or length.");
    }
    const auto index = IndexOf(message.sender);
    if (hash_tick <= verified_tick) {
        return;
    }
    if (hash_tick > tick && hash_tick - tick > settings.max_unchecked_ticks) {
        failure = "Peer " + std::to_string(message.sender.value) +
                  " sent a state hash beyond the checkpoint limit.";
        return;
    }
    const auto [entry, inserted] =
        checkpoints[hash_tick].remote.emplace(index, hash);
    if (!inserted && entry->second != hash) {
        failure = "Peer " + std::to_string(message.sender.value) +
                  " changed its state hash for tick " +
                  std::to_string(hash_tick) + ".";
    }
}

template <typename Input, typename Use>
void InputSync<Input, Use>::CheckStateHashes() {
    const auto confirmed = ConfirmedTick();
    while (confirmed - hashed_tick >= settings.hash_interval_ticks) {
        const auto checkpoint_tick = hashed_tick + settings.hash_interval_ticks;
        auto &checkpoint = checkpoints[checkpoint_tick];
        if (checkpoint_tick == tick) {
            checkpoint.local = HashBytes(SaveWorld().bytes);
        } else {
            const auto record = history.find(checkpoint_tick);
            if (record == history.end()) {
                failure = "State comparison requires a snapshot outside "
                          "retained history.";
                return;
            }
            checkpoint.local = HashBytes(record->second.before.bytes);
        }
        hashed_tick = checkpoint_tick;
    }
    for (auto &[checkpoint_tick, checkpoint] : checkpoints) {
        if (!checkpoint.local) {
            continue;
        }
        if (!checkpoint.sent) {
            MessageWriter writer;
            writer.WriteUint64(checkpoint_tick);
            writer.WriteUint64(*checkpoint.local);
            if (!peers.Broadcast(
                    static_cast<MessageType>(InputSyncMessageType::StateHash),
                    writer.Finish())) {
                return;
            }
            checkpoint.sent = true;
        }
        for (const auto &[index, hash] : checkpoint.remote) {
            if (hash != *checkpoint.local) {
                failure = "State mismatch with peer " +
                          std::to_string(players[index].peer.value) +
                          " at tick " + std::to_string(checkpoint_tick) +
                          ". Local hash=" + std::to_string(*checkpoint.local) +
                          " remote hash=" + std::to_string(hash) +
                          ". Simulation paused.";
                return;
            }
        }
    }
    while (!checkpoints.empty()) {
        const auto first = checkpoints.begin();
        const auto &checkpoint = first->second;
        if (first->first != verified_tick + settings.hash_interval_ticks ||
            !checkpoint.sent || checkpoint.remote.size() != players.size() - 1) {
            break;
        }
        verified_tick = first->first;
        checkpoints.erase(first);
    }
}

template <typename Input, typename Use>
void InputSync<Input, Use>::RecordInput(std::size_t index,
                                        std::uint64_t input_tick,
                                        const Input &input) {
    auto &player = players[index];
    const auto [found, inserted] = player.actual.emplace(input_tick, input);
    if (!inserted && EncodedBytes(found->second) != EncodedBytes(input)) {
        failure = "Peer " + std::to_string(player.peer.value) +
                  " changed its input for tick " + std::to_string(input_tick) +
                  ".";
        return;
    }
    while (player.actual.contains(player.next_input_tick)) {
        ++player.next_input_tick;
    }
    player.latest_input_tick =
        std::max(player.latest_input_tick, input_tick + 1);
    const auto record = history.find(input_tick);
    if (record != history.end() &&
        simulation.ChangesStep(record->second.used[index], input,
                               record->second.use[index])) {
        correction_tick = correction_tick
                              ? std::min(*correction_tick, input_tick)
                              : input_tick;
    }
}

template <typename Input, typename Use>
Input InputSync<Input, Use>::InputFor(std::size_t index,
                                      std::uint64_t input_tick) const {
    const auto &player = players[index];
    const auto actual = player.actual.find(input_tick);
    if (actual != player.actual.end()) {
        return actual->second;
    }
    auto last = player.preceding;
    auto previous = player.actual.lower_bound(input_tick);
    if (previous != player.actual.begin()) {
        --previous;
        last = previous->second;
    }
    return simulation.Predict(last);
}

template <typename Input, typename Use>
NetworkMessage InputSync<Input, Use>::EncodeTickInput(std::uint64_t input_tick,
                                                      const Input &input) const {
    MessageWriter writer;
    writer.WriteUint64(input_tick);
    simulation.EncodeInput(writer, input);
    return writer.Finish();
}

template <typename Input, typename Use>
std::vector<std::byte>
InputSync<Input, Use>::EncodedBytes(const Input &input) const {
    MessageWriter writer;
    simulation.EncodeInput(writer, input);
    return writer.Finish().bytes;
}

template <typename Input, typename Use>
NetworkMessage InputSync<Input, Use>::SaveWorld() {
    MessageWriter writer;
    writer.WriteUint64(tick);
    writer.WriteBytes(simulation.Save().bytes);
    return writer.Finish();
}

template <typename Input, typename Use>
void InputSync<Input, Use>::RestoreWorld(std::span<const std::byte> saved) {
    MessageReader reader(saved);
    tick = reader.ReadUint64();
    simulation.Load(reader.ReadBytes(reader.Remaining()));
}

template <typename Input, typename Use>
void InputSync<Input, Use>::ApplyCorrection() {
    if (!correction_tick) {
        return;
    }
    const auto restore = history.find(*correction_tick);
    if (restore == history.end()) {
        failure = "Correction requires a snapshot outside retained history.";
        return;
    }
    const auto target = tick;
    last_rollback_depth = target - *correction_tick;
    RestoreWorld(restore->second.before.bytes);
    correction_tick.reset();
    while (tick < target) {
        AdvanceTick();
    }
    ++rollback_count;
}

template <typename Input, typename Use>
void InputSync<Input, Use>::AdvanceTick() {
    if (tick == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("InputSync exhausted its tick counter.");
    }
    std::vector<Input> inputs;
    inputs.reserve(players.size());
    for (std::size_t index = 0; index < players.size(); ++index) {
        inputs.push_back(InputFor(index, tick));
    }
    auto before = SaveWorld();
    auto use = simulation.Step(inputs);
    if (use.size() != players.size()) {
        throw std::logic_error("SyncedSimulation::Step must report one use "
                               "per player.");
    }
    history.insert_or_assign(
        tick, TickRecord{std::move(before), std::move(inputs), std::move(use)});
    ++tick;
}

template <typename Input, typename Use>
void InputSync<Input, Use>::PruneHistory() {
    const auto oldest =
        tick > settings.history_ticks ? tick - settings.history_ticks : 0;
    history.erase(history.begin(), history.lower_bound(oldest));
    for (auto &player : players) {
        const auto first_kept = player.actual.lower_bound(oldest);
        if (first_kept != player.actual.begin()) {
            auto previous = first_kept;
            --previous;
            player.preceding = previous->second;
        }
        player.actual.erase(player.actual.begin(), first_kept);
    }
}

template <typename Input, typename Use>
bool InputSync<Input, Use>::CanPredict() const {
    for (std::size_t index = 0; index < players.size(); ++index) {
        if (index != local_index &&
            players[index].next_input_tick + settings.prediction_ticks <= tick) {
            return false;
        }
    }
    return true;
}

template <typename Input, typename Use>
TicCount InputSync<Input, Use>::PacingStepTics() const {
    for (std::size_t index = 0; index < players.size(); ++index) {
        if (index == local_index) {
            continue;
        }
        const auto latest = players[index].latest_input_tick;
        if (tick > latest && tick - latest > settings.pacing_lead_ticks) {
            return 2 * settings.step_tics;
        }
    }
    return settings.step_tics;
}

template <typename Input, typename Use>
std::string InputSync<Input, Use>::Status() const {
    if (peers.HasFailed()) {
        return peers.Status();
    }
    if (!failure.empty()) {
        return failure;
    }
    if (!peers.IsRunning() || peers.IsChangingRoster()) {
        return peers.Status();
    }
    return waiting.empty() ? "Playing." : waiting;
}

template <typename Input, typename Use>
std::uint64_t InputSync<Input, Use>::Tick() const {
    return tick;
}

template <typename Input, typename Use>
std::uint64_t InputSync<Input, Use>::ConfirmedTick() const {
    auto confirmed = tick;
    for (const auto &player : players) {
        confirmed = std::min(confirmed, player.next_input_tick);
    }
    return confirmed;
}

template <typename Input, typename Use>
std::string InputSync<Input, Use>::Diagnostics() const {
    std::string result = "roster=" + std::to_string(peers.Revision()) +
                         " tick=" + std::to_string(tick) +
                         " confirmed=" + std::to_string(ConfirmedTick()) +
                         " verified=" + std::to_string(verified_tick);
    for (std::size_t index = 0; index < players.size(); ++index) {
        if (index == local_index) {
            continue;
        }
        const auto latest = players[index].latest_input_tick;
        result += " peer" + std::to_string(players[index].peer.value) + "=" +
                  std::to_string(latest) + " lead=";
        result += tick >= latest ? std::to_string(tick - latest)
                                 : "-" + std::to_string(latest - tick);
    }
    return result + " rollbacks=" + std::to_string(rollback_count) +
           " last-depth=" + std::to_string(last_rollback_depth);
}

template <typename Input, typename Use>
void InputSync<Input, Use>::RequestLeave() {
    if (!failure.empty()) {
        force_close = true;
    } else {
        peers.RequestLeave();
    }
}

template <typename Input, typename Use>
bool InputSync<Input, Use>::HasDeparted() const {
    return peers.HasDeparted();
}

template <typename Input, typename Use>
bool InputSync<Input, Use>::CanClose() const {
    return force_close || peers.CanClose();
}

template <typename Input, typename Use>
std::uint64_t InputSync<Input, Use>::Freeze() {
    pending_tics = 0;
    unsent_input.reset();
    return tick;
}

template <typename Input, typename Use>
std::uint64_t InputSync<Input, Use>::ProgressOf(PeerId peer) {
    return players[IndexOf(peer)].next_input_tick;
}

template <typename Input, typename Use>
void InputSync<Input, Use>::Abandon(PeerId peer, std::uint64_t cutoff,
                                    std::uint64_t boundary) {
    const auto index = IndexOf(peer);
    auto &player = players[index];
    player.actual.erase(player.actual.lower_bound(cutoff), player.actual.end());
    player.next_input_tick = std::min(player.next_input_tick, cutoff);
    for (auto input_tick = cutoff; input_tick < boundary; ++input_tick) {
        RecordInput(index, input_tick, Input{});
    }
}

template <typename Input, typename Use>
bool InputSync<Input, Use>::SettleAt(std::uint64_t boundary) {
    ReceiveMessages();
    if (!failure.empty()) {
        return false;
    }
    ApplyCorrection();
    if (!failure.empty()) {
        return false;
    }
    pending_tics = 0;
    for (std::uint32_t step = 0;
         tick < boundary && step < settings.steps_per_frame; ++step) {
        const Input neutral{};
        if (!peers.Broadcast(static_cast<MessageType>(InputSyncMessageType::Input),
                             EncodeTickInput(tick, neutral))) {
            return false;
        }
        RecordInput(local_index, tick, neutral);
        AdvanceTick();
    }
    return tick == boundary && ConfirmedTick() == boundary;
}

template <typename Input, typename Use>
void InputSync<Input, Use>::ChangeRoster(std::span<const PeerId> departing,
                                         std::span<const PeerId> joining) {
    for (const auto peer : departing) {
        simulation.RemovePlayer(peer);
    }
    for (const auto peer : joining) {
        simulation.AddPlayer(peer);
    }
    ResetPlayers();
    unsent_input.reset();
    started = false;
    waiting = "Roster change complete. " + std::to_string(players.size()) +
              " player(s) in the world.";
}

template <typename Input, typename Use>
void InputSync<Input, Use>::LoadWorld(std::span<const PeerId> roster,
                                      std::span<const std::byte> world) {
    for (const auto peer : roster) {
        simulation.AddPlayer(peer);
    }
    RestoreWorld(world);
    ResetPlayers();
}

} // namespace svanes

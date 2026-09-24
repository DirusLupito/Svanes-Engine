#include "goose_rollback.hpp"

#include <svanes/game.hpp>
#include <svanes/input.hpp>
#include <svanes/network/message_serialization.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint64_t PredictionTicks = 30;
constexpr std::uint64_t PacingLeadTicks = 3;
constexpr std::uint64_t HistoryTicks = 256;
constexpr std::uint64_t HashIntervalTicks = 100;
constexpr std::uint64_t MaximumUncheckedTicks = 200;
constexpr std::uint32_t StepsPerFrame = 8;
constexpr std::uint32_t DoubleTapTicks = 25;
constexpr svanes::TicCount MaximumBacklog = 25 * GooseStepTics;

/**
 * Compares the controls that affect the movement simulation.
 * @param a The first input.
 * @param b The second input.
 * @return Whether both inputs produce the same movement controls.
 */
bool SameMovement(const GooseIntent& a, const GooseIntent& b)
{
    return a.move.x == b.move.x && a.dash == b.dash && a.jump == b.jump;
}

/**
 * Serializes one tick's movement input using direction bytes 0, 1, and 2.
 * @param tick The simulation tick.
 * @param input The local movement controls.
 * @return The game payload for an Input message.
 */
svanes::NetworkMessage EncodeInput(std::uint64_t tick, const GooseIntent& input)
{
    svanes::MessageWriter writer;
    writer.WriteUint64(tick);
    writer.WriteUint8(static_cast<std::uint8_t>(input.move.x + 1.0F));
    writer.WriteUint8(static_cast<std::uint8_t>(input.dash + 1.0F));
    writer.WriteBool(input.jump);
    return writer.Finish();
}

}

GooseRollback::GooseRollback(GooseSimulation& simulation, GooseNetwork& network)
    : simulation(simulation), network(network)
{
    if (simulation.Tick() != 0) {
        throw std::invalid_argument("GooseRollback requires a simulation at tick zero.");
    }
    const auto roster = network.Peers();
    const auto local = std::find(roster.begin(), roster.end(), network.LocalPeer());
    if (local == roster.end()) {
        throw std::invalid_argument("GooseRollback local peer is absent from the roster.");
    }
    local_index = static_cast<std::size_t>(local - roster.begin());
    for (const auto peer : roster) {
        players.push_back({peer, {}, {}, 0});
    }
}

void GooseRollback::ReceiveMessages()
{
    svanes::SessionMessage message;
    while (network.Receive(message)) {
        if (message.type == static_cast<svanes::MessageType>(GooseMessageType::StateHash)) {
            ReceiveStateHash(message);
            if (!failure.empty()) {
                return;
            }
            continue;
        }
        if (message.type != static_cast<svanes::MessageType>(GooseMessageType::Input)) {
            throw std::invalid_argument("GooseRollback received an unexpected game message type.");
        }
        svanes::MessageReader reader(message.payload);
        const auto tick = reader.ReadUint64();
        const auto move = reader.ReadUint8();
        const auto dash = reader.ReadUint8();
        const bool jump = reader.ReadBool();
        if (move > 2 || dash > 2 || reader.Remaining() != 0 ||
            tick == std::numeric_limits<std::uint64_t>::max()) {
            throw std::invalid_argument("Goose input has an invalid direction, tick, or length.");
        }
        const auto found = std::find_if(players.begin(), players.end(),
            [&](const auto& player) { return player.peer == message.sender; });
        if (found == players.end() || message.sender == network.LocalPeer()) {
            throw std::invalid_argument("Goose input has an invalid sender.");
        }
        const auto current = simulation.Tick();
        if (tick > current && tick - current > HistoryTicks) {
            failure = "Peer " + std::to_string(message.sender.value) + " sent input beyond the history limit.";
            return;
        }
        if (tick < current && current - tick > HistoryTicks) {
            if (tick >= found->next_input_tick) {
                failure = "A missing input is older than retained history.";
                return;
            }
            continue;
        }
        GooseIntent input{};
        input.move.x = static_cast<float>(move) - 1.0F;
        input.dash = static_cast<float>(dash) - 1.0F;
        input.jump = jump;
        RecordInput(static_cast<std::size_t>(found - players.begin()), tick, input);
        if (!failure.empty()) {
            return;
        }
    }
}

void GooseRollback::ReceiveStateHash(const svanes::SessionMessage& message)
{
    svanes::MessageReader reader(message.payload);
    const auto tick = reader.ReadUint64();
    const auto hash = reader.ReadUint64();
    const auto found = std::find_if(players.begin(), players.end(),
        [&](const auto& player) { return player.peer == message.sender; });
    if (reader.Remaining() != 0 || tick == 0 || tick % HashIntervalTicks != 0 ||
        found == players.end() || message.sender == network.LocalPeer()) {
        throw std::invalid_argument("Goose state hash has an invalid sender, tick, or length.");
    }
    if (tick <= verified_tick) {
        return;
    }
    if (tick > simulation.Tick() && tick - simulation.Tick() > MaximumUncheckedTicks) {
        failure = "Peer " + std::to_string(message.sender.value) + " sent a state hash beyond the checkpoint limit.";
        return;
    }
    const auto index = static_cast<std::size_t>(found - players.begin());
    const auto [entry, inserted] = checkpoints[tick].remote.emplace(index, hash);
    if (!inserted && entry->second != hash) {
        failure = "Peer " + std::to_string(message.sender.value) + " changed its state hash for tick " + std::to_string(tick) + ".";
    }
}

void GooseRollback::CheckStateHashes(const svanes::FrameContext& frame)
{
    const auto confirmed = ConfirmedTick();
    while (confirmed - hashed_tick >= HashIntervalTicks) {
        const auto tick = hashed_tick + HashIntervalTicks;
        auto& checkpoint = checkpoints[tick];
        if (tick == simulation.Tick()) {
            checkpoint.local = GooseSimulation::Hash(simulation.Capture(frame.world));
        } else {
            const auto record = history.find(tick);
            if (record == history.end()) {
                failure = "State comparison requires a snapshot outside retained history.";
                return;
            }
            checkpoint.local = GooseSimulation::Hash(record->second.before);
        }
        hashed_tick = tick;
    }
    for (auto& [tick, checkpoint] : checkpoints) {
        if (!checkpoint.local) {
            continue;
        }
        if (!checkpoint.sent) {
            svanes::MessageWriter writer;
            writer.WriteUint64(tick);
            writer.WriteUint64(*checkpoint.local);
            if (!network.Broadcast(GooseMessageType::StateHash, writer.Finish())) {
                return;
            }
            checkpoint.sent = true;
        }
        for (const auto& [index, hash] : checkpoint.remote) {
            if (hash != *checkpoint.local) {
                failure = "State mismatch with peer " + std::to_string(players[index].peer.value) +
                    " at tick " + std::to_string(tick) + ". Local hash=" +
                    std::to_string(*checkpoint.local) + " remote hash=" + std::to_string(hash) + ". Simulation paused.";
                return;
            }
        }
    }
    while (!checkpoints.empty()) {
        const auto first = checkpoints.begin();
        const auto& checkpoint = first->second;
        if (first->first != verified_tick + HashIntervalTicks || !checkpoint.sent ||
            checkpoint.remote.size() != players.size() - 1) {
            break;
        }
        verified_tick = first->first;
        checkpoints.erase(first);
    }
}

void GooseRollback::RecordInput(std::size_t index, std::uint64_t tick, const GooseIntent& input)
{
    auto& player = players[index];
    const auto [found, inserted] = player.actual.emplace(tick, input);
    if (!inserted && !SameMovement(found->second, input)) {
        failure = "Peer " + std::to_string(player.peer.value) + " changed its input for tick " + std::to_string(tick) + ".";
        return;
    }
    while (player.actual.contains(player.next_input_tick)) {
        ++player.next_input_tick;
    }
    player.latest_input_tick = std::max(player.latest_input_tick, tick + 1);
    const auto record = history.find(tick);
    if (record != history.end() && !SameMovement(record->second.used[index], input)) {
        correction_tick = correction_tick ? std::min(*correction_tick, tick) : tick;
    }
}

GooseIntent GooseRollback::InputFor(std::size_t index, std::uint64_t tick) const
{
    const auto& player = players[index];
    const auto actual = player.actual.find(tick);
    if (actual != player.actual.end()) {
        return actual->second;
    }
    auto input = player.preceding;
    auto previous = player.actual.lower_bound(tick);
    if (previous != player.actual.begin()) {
        --previous;
        input = previous->second;
    }
    // Dash is a press event, while walking and jumping are held controls.
    input.dash = 0.0F;
    return input;
}

GooseIntent GooseRollback::TakeLocalInput()
{
    if (controls.left_tap_ticks > 0) {
        --controls.left_tap_ticks;
    }
    if (controls.right_tap_ticks > 0) {
        --controls.right_tap_ticks;
    }
    GooseIntent input{};
    input.move.x = controls.move;
    input.jump = controls.jump;
    if (controls.left_pressed) {
        if (controls.left_tap_ticks > 0) {
            input.dash = -1.0F;
            controls.left_tap_ticks = 0;
        } else {
            controls.left_tap_ticks = DoubleTapTicks;
        }
    }
    if (controls.right_pressed) {
        if (controls.right_tap_ticks > 0) {
            input.dash = 1.0F;
            controls.right_tap_ticks = 0;
        } else {
            controls.right_tap_ticks = DoubleTapTicks;
        }
    }
    controls.left_pressed = false;
    controls.right_pressed = false;
    return input;
}

void GooseRollback::AdvanceTick(const svanes::FrameContext& frame)
{
    std::vector<GooseIntent> inputs;
    inputs.reserve(players.size());
    for (std::size_t index = 0; index < players.size(); ++index) {
        inputs.push_back(InputFor(index, simulation.Tick()));
    }
    history.insert_or_assign(simulation.Tick(), TickRecord{simulation.Capture(frame.world), inputs});
    simulation.Step(frame.world, frame.gravity, inputs);
}

void GooseRollback::PruneHistory()
{
    const auto oldest = simulation.Tick() > HistoryTicks ? simulation.Tick() - HistoryTicks : 0;
    history.erase(history.begin(), history.lower_bound(oldest));
    for (auto& player : players) {
        const auto first_kept = player.actual.lower_bound(oldest);
        if (first_kept != player.actual.begin()) {
            auto previous = first_kept;
            --previous;
            player.preceding = previous->second;
        }
        player.actual.erase(player.actual.begin(), first_kept);
    }
}

void GooseRollback::Update(const svanes::FrameContext& frame)
{
    if (network.HasFailed() || !failure.empty()) {
        return;
    }
    ReceiveMessages();
    if (!failure.empty()) {
        return;
    }
    if (!network.IsReady()) {
        pending_tics = 0;
        controls = {};
        return;
    }
    const bool first_frame = !started;
    started = true;

    if (correction_tick) {
        const auto restore = history.find(*correction_tick);
        if (restore == history.end()) {
            failure = "Correction requires a snapshot outside retained history.";
            return;
        }
        const auto target = simulation.Tick();
        last_rollback_depth = target - *correction_tick;
        simulation.Restore(frame.world, restore->second.before);
        correction_tick.reset();
        while (simulation.Tick() < target) {
            AdvanceTick(frame);
        }
        ++rollback_count;
    }

    CheckStateHashes(frame);
    if (!failure.empty()) {
        return;
    }

    controls.move = (frame.input.IsDown(svanes::Key::D) ? 1.0F : 0.0F)
        - (frame.input.IsDown(svanes::Key::A) ? 1.0F : 0.0F);
    controls.jump = frame.input.IsDown(svanes::Key::Space);
    controls.left_pressed |= frame.input.WasPressed(svanes::Key::A);
    controls.right_pressed |= frame.input.WasPressed(svanes::Key::D);
    if (!first_frame) {
        pending_tics += std::min(frame.real_delta_tics, MaximumBacklog - pending_tics);
    }
    waiting.clear();
    for (std::uint32_t step = 0; step < StepsPerFrame; ++step) {
        const auto pacing_tics = PacingStepTics();
        if (pending_tics < pacing_tics) {
            break;
        }
        if (simulation.Tick() - verified_tick >= MaximumUncheckedTicks) {
            waiting = "Waiting for peer state hashes.";
            pending_tics = std::min(pending_tics, GooseStepTics);
            break;
        }
        if (simulation.Tick() - ConfirmedTick() >= PredictionTicks) {
            waiting = "Waiting for remote inputs.";
            pending_tics = std::min(pending_tics, GooseStepTics);
            break;
        }
        const auto saved_controls = controls;
        const auto input = TakeLocalInput();
        if (!network.Broadcast(GooseMessageType::Input, EncodeInput(simulation.Tick(), input))) {
            controls = saved_controls;
            waiting = "Waiting for outgoing message capacity.";
            pending_tics = std::min(pending_tics, GooseStepTics);
            break;
        }
        RecordInput(local_index, simulation.Tick(), input);
        AdvanceTick(frame);
        pending_tics -= pacing_tics;
    }
    CheckStateHashes(frame);
    PruneHistory();
}

svanes::TicCount GooseRollback::PacingStepTics() const
{
    for (std::size_t index = 0; index < players.size(); ++index) {
        if (index == local_index) {
            continue;
        }
        const auto latest = players[index].latest_input_tick;
        if (simulation.Tick() > latest && simulation.Tick() - latest > PacingLeadTicks) {
            return 2 * GooseStepTics;
        }
    }
    return GooseStepTics;
}

std::string GooseRollback::Status() const
{
    if (network.HasFailed()) {
        return network.Status();
    }
    if (!failure.empty()) {
        return failure;
    }
    if (!network.IsReady()) {
        return network.Status();
    }
    return waiting.empty() ? "Playing." : waiting;
}

std::uint64_t GooseRollback::ConfirmedTick() const
{
    auto confirmed = simulation.Tick();
    for (const auto& player : players) {
        confirmed = std::min(confirmed, player.next_input_tick);
    }
    return confirmed;
}

std::uint64_t GooseRollback::RollbackCount() const
{
    return rollback_count;
}

std::string GooseRollback::Diagnostics() const
{
    std::string result = "tick=" + std::to_string(simulation.Tick()) +
        " confirmed=" + std::to_string(ConfirmedTick()) +
        " verified=" + std::to_string(verified_tick);
    for (std::size_t index = 0; index < players.size(); ++index) {
        if (index == local_index) {
            continue;
        }
        const auto latest = players[index].latest_input_tick;
        result += " peer" + std::to_string(players[index].peer.value) +
            "=" + std::to_string(latest) + " lead=";
        result += simulation.Tick() >= latest
            ? std::to_string(simulation.Tick() - latest)
            : "-" + std::to_string(latest - simulation.Tick());
    }
    return result + " rollbacks=" + std::to_string(rollback_count) +
        " last-depth=" + std::to_string(last_rollback_depth);
}

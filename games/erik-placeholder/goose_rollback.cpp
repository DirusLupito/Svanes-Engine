#include "goose_rollback.hpp"

#include <svanes/game.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/network/message_serialization.hpp>
#include <svanes/registry.hpp>

#include <algorithm>
#include <cmath>
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
constexpr auto AssignmentRetention = std::chrono::seconds(15);

/**
 * Compares movement and firing controls, including aim while firing.
 * @param a The first input.
 * @param b The second input.
 * @return Whether both inputs produce the same controls.
 */
bool SameInput(const GooseIntent& a, const GooseIntent& b)
{
    return a.move.x == b.move.x && a.dash == b.dash && a.jump == b.jump &&
        a.fire == b.fire && (!a.fire || (a.aim.x == b.aim.x && a.aim.y == b.aim.y));
}

/**
 * Serializes one tick's controls, with movement directions encoded as 0, 1, and 2.
 * @param tick The simulation tick.
 * @param input The local movement, firing, and goose-relative aim controls.
 * @return The game payload for an Input message.
 */
svanes::NetworkMessage EncodeInput(std::uint64_t tick, const GooseIntent& input)
{
    svanes::MessageWriter writer;
    writer.WriteUint64(tick);
    writer.WriteUint8(static_cast<std::uint8_t>(input.move.x + 1.0F));
    writer.WriteUint8(static_cast<std::uint8_t>(input.dash + 1.0F));
    writer.WriteBool(input.jump);
    writer.WriteBool(input.fire);
    writer.WriteFloat32(input.aim.x);
    writer.WriteFloat32(input.aim.y);
    return writer.Finish();
}

}

GooseRollback::GooseRollback(GooseSimulation& simulation, GooseNetwork& network)
    : simulation(simulation), network(network)
{
    const auto roster = simulation.Roster();
    if (!std::equal(roster.begin(), roster.end(), network.Peers().begin(), network.Peers().end())) {
        throw std::invalid_argument("GooseRollback requires matching simulation and network rosters.");
    }
    next_peer_id = roster.back().value + 1;
    ResetPlayers();
}

GooseRollback::GooseRollback(GooseSimulation& simulation, GooseNetwork& network, svanes::Registry& world,
                             const svanes::NetworkMessage& snapshot)
    : simulation(simulation), network(network)
{
    svanes::MessageReader reader(snapshot);
    next_peer_id = reader.ReadUint32();
    const auto count = reader.ReadUint32();
    if (count == 0 || count > GooseMaxPlayers) {
        throw std::invalid_argument("Joined world snapshot has an invalid player count.");
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        simulation.AddPlayer(world, {reader.ReadUint32()});
    }
    const auto roster = simulation.Roster();
    if (!std::equal(roster.begin(), roster.end(), network.Peers().begin(), network.Peers().end())) {
        throw std::runtime_error("The received world's players differ from the members this process was told about.");
    }
    const auto received = simulation.Decode(reader);
    const auto hash = reader.ReadUint64();
    if (reader.Remaining() != 0 || next_peer_id <= roster.back().value) {
        throw std::invalid_argument("Joined world snapshot has trailing data or an invalid next id.");
    }
    simulation.Restore(world, received);
    if (GooseSimulation::Hash(simulation.Capture(world)) != hash) {
        throw std::runtime_error("The rebuilt world does not match the sponsor's world hash.");
    }
    ResetPlayers();
}

std::uint64_t GooseRollback::RulesHash()
{
    svanes::MessageWriter writer;
    writer.WriteUint64(GooseStepTics);
    writer.WriteFloat32(GooseGravity);
    writer.WriteUint64(HashIntervalTicks);
    writer.WriteUint64(GooseMaxPlayers);
    return svanes::HashBytes(writer.Finish().bytes);
}

void GooseRollback::ResetPlayers()
{
    const auto tick = simulation.Tick();
    players.clear();
    for (const auto peer : network.Peers()) {
        players.push_back({peer, {}, {}, tick, tick});
    }
    const auto local = std::find(network.Peers().begin(), network.Peers().end(), network.LocalPeer());
    if (local == network.Peers().end()) {
        throw std::logic_error("GooseRollback local peer is absent from the roster.");
    }
    local_index = static_cast<std::size_t>(local - network.Peers().begin());
    history.clear();
    checkpoints.clear();
    correction_tick.reset();
    hashed_tick = tick - tick % HashIntervalTicks;
    verified_tick = hashed_tick;
}

void GooseRollback::ReceiveMessages()
{
    svanes::SessionMessage message;
    while (network.Receive(message)) {
        if (message.type == static_cast<svanes::MessageType>(GooseMessageType::RosterStop) ||
            message.type == static_cast<svanes::MessageType>(GooseMessageType::RosterPrepared)) {
            ReceiveRosterMessage(message);
            if (!failure.empty()) {
                return;
            }
            continue;
        }
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
        const bool fire = reader.ReadBool();
        const svanes::Vector2D aim{reader.ReadFloat32(), reader.ReadFloat32()};
        if (move > 2 || dash > 2 || !std::isfinite(aim.x) || !std::isfinite(aim.y) ||
            std::abs(aim.x) > 100000.0F || std::abs(aim.y) > 100000.0F || reader.Remaining() != 0 ||
            tick == std::numeric_limits<std::uint64_t>::max()) {
            throw std::invalid_argument("Goose input has an invalid direction, aim, tick, or length.");
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
        input.fire = fire;
        input.aim = aim;
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
    if (!inserted && !SameInput(found->second, input)) {
        failure = "Peer " + std::to_string(player.peer.value) + " changed its input for tick " + std::to_string(tick) + ".";
        return;
    }
    while (player.actual.contains(player.next_input_tick)) {
        ++player.next_input_tick;
    }
    player.latest_input_tick = std::max(player.latest_input_tick, tick + 1);
    const auto record = history.find(tick);
    if (record != history.end() && ChangesStep(record->second.used[index], input, record->second.use[index])) {
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
    // Repeat held controls and aim, but never repeat a dash press.
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
    input.fire = controls.fire;
    input.aim = controls.aim;
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
    const auto tick = simulation.Tick();
    auto before = simulation.Capture(frame.world);
    auto use = simulation.Step(frame.world, frame.gravity, inputs);
    history.insert_or_assign(tick, TickRecord{std::move(before), std::move(inputs), std::move(use)});
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
    if (departed || network.HasFailed() || !failure.empty()) {
        return;
    }
    ReceiveMessages();
    if (!failure.empty()) {
        return;
    }
    HandleJoinRequests();
    if ((leave_requested || !join_requests.empty()) && !roster_pause) {
        BeginRosterPause();
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

    if (roster_pause) {
        UpdateRosterPause(frame);
        return;
    }

    CheckStateHashes(frame);
    if (!failure.empty()) {
        return;
    }

    controls.move = (frame.input.IsDown(svanes::Key::D) ? 1.0F : 0.0F)
        - (frame.input.IsDown(svanes::Key::A) ? 1.0F : 0.0F);
    controls.jump = frame.input.IsDown(svanes::Key::Space);
    controls.fire = frame.input.IsMouseButtonDown(svanes::MouseButton::Left);
    controls.aim = {};
    if (controls.fire) {
        const auto mouse = frame.input.MousePosition();
        const auto aim = frame.camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});
        const auto& goose = frame.world.GetComponent<svanes::Transform>(simulation.PlayerEntity(network.LocalPeer()));
        controls.aim = {std::clamp(aim.x - goose.x, -100000.0F, 100000.0F),
            std::clamp(aim.y - goose.y, -100000.0F, 100000.0F)};
    }
    controls.left_pressed |= frame.input.WasPressed(svanes::Key::A);
    controls.right_pressed |= frame.input.WasPressed(svanes::Key::D);
    if (!first_frame) {
        pending_tics += std::min(frame.real_delta_tics, MaximumBacklog - pending_tics);
    }
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
        waiting.clear();
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
    if (departed) {
        return "Departure agreed. Finishing final message delivery.";
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
    std::string result = "roster=" + std::to_string(network.Revision()) + " tick=" + std::to_string(simulation.Tick()) +
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

void GooseRollback::RequestLeave()
{
    if (network.HasFailed() || !failure.empty()) {
        force_close = true;
    } else {
        leave_requested = true;
    }
}

bool GooseRollback::HasDeparted() const
{
    return departed;
}

bool GooseRollback::CanClose() const
{
    return force_close || (departed && network.OutgoingDrained());
}

void GooseRollback::BeginRosterPause()
{
    if (!roster_pause) {
        RosterPause pause{{simulation.Tick(), leave_requested, {}}, {}, {}, {}, false,
            std::chrono::steady_clock::now()};
        if (!leave_requested) {
            pause.sponsored = std::move(join_requests);
            for (const auto& candidate : pause.sponsored) {
                pause.local_stop.joining.push_back(candidate.address);
            }
        }
        join_requests.clear();
        roster_pause = std::move(pause);
        pending_tics = 0;
        controls = {};
    }
}

void GooseRollback::HandleJoinRequests()
{
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(assignments, [&](const auto& entry) { return now - entry.second.sent_at > AssignmentRetention; });
    svanes::StrangerMessage stranger;
    while (network.ReceiveStranger(stranger)) {
        if (stranger.message.type != static_cast<svanes::MessageType>(GooseMessageType::JoinRequest)) {
            continue;
        }
        const auto connection = stranger.connection;
        const auto assigned = assignments.find(connection.value);
        if (assigned != assignments.end()) {
            network.SendStranger(connection, GooseMessageType::JoinAssigned, assigned->second.payload);
            continue;
        }
        const auto same = [&](const JoinCandidate& candidate) { return candidate.connection == connection; };
        const auto sponsored = roster_pause ? roster_pause->sponsored.size() : 0;
        if (std::any_of(join_requests.begin(), join_requests.end(), same) ||
            (roster_pause && std::any_of(roster_pause->sponsored.begin(), roster_pause->sponsored.end(), same))) {
            network.SendStranger(connection, GooseMessageType::JoinPending, {});
            continue;
        }
        if (stranger.message.payload.bytes.size() != sizeof(std::uint64_t)) {
            RejectJoin(connection, "malformed join request");
            continue;
        }
        svanes::MessageReader reader(stranger.message.payload);
        if (reader.ReadUint64() != RulesHash()) {
            RejectJoin(connection, "incompatible game version");
        } else if (leave_requested || departed) {
            RejectJoin(connection, "the player you contacted is leaving");
        } else if (network.ActivePeerAt(connection)) {
            RejectJoin(connection, "that address is already in the game");
        } else if (players.size() + join_requests.size() + sponsored >= GooseMaxPlayers) {
            RejectJoin(connection, "the game is full");
        } else {
            join_requests.push_back({connection, network.Address(connection)});
            network.SendStranger(connection, GooseMessageType::JoinPending, {});
        }
    }
}

void GooseRollback::RejectJoin(svanes::ConnectionId connection, std::string_view reason)
{
    svanes::MessageWriter writer;
    writer.WriteText(reason);
    network.SendStranger(connection, GooseMessageType::JoinRejected, writer.Finish());
}

svanes::NetworkMessage GooseRollback::EncodeJoinSnapshot(const svanes::Registry& world) const
{
    svanes::MessageWriter writer;
    writer.WriteUint32(next_peer_id);
    const auto roster = simulation.Roster();
    writer.WriteUint32(static_cast<std::uint32_t>(roster.size()));
    for (const auto peer : roster) {
        writer.WriteUint32(peer.value);
    }
    const auto snapshot = simulation.Capture(world);
    GooseSimulation::Encode(writer, snapshot);
    writer.WriteUint64(GooseSimulation::Hash(snapshot));
    return writer.Finish();
}

void GooseRollback::ReceiveRosterMessage(const svanes::SessionMessage& message)
{
    if (message.sender == network.LocalPeer() ||
        std::find(network.Peers().begin(), network.Peers().end(), message.sender) == network.Peers().end()) {
        throw std::invalid_argument("Roster message has an invalid sender.");
    }
    BeginRosterPause();
    svanes::MessageReader reader(message.payload);
    const auto tick = reader.ReadUint64();
    const auto current = simulation.Tick();
    if ((tick > current && tick - current > HistoryTicks) ||
        (tick < current && current - tick > HistoryTicks)) {
        failure = "Roster pause is outside the retained tick range.";
        return;
    }
    if (message.type == static_cast<svanes::MessageType>(GooseMessageType::RosterStop)) {
        StopRecord record{tick, reader.ReadBool(), {}};
        const auto count = reader.ReadUint32();
        if (count > GooseMaxPlayers) {
            throw std::invalid_argument("Roster stop admits too many players.");
        }
        for (std::uint32_t index = 0; index < count; ++index) {
            record.joining.push_back(svanes::ReadAddress(reader));
        }
        const auto [entry, inserted] = roster_pause->stops.emplace(message.sender.value, record);
        if (!inserted && entry->second != record) {
            failure = "Peer changed its roster stop announcement.";
        }
    } else {
        const PreparedRecord record{tick, reader.ReadUint64(), reader.ReadUint64()};
        const auto [entry, inserted] = roster_pause->prepared.emplace(message.sender.value, record);
        if (!inserted && entry->second != record) {
            failure = "Peer changed its prepared roster state.";
        }
    }
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("Roster message has trailing data.");
    }
}

void GooseRollback::UpdateRosterPause(const svanes::FrameContext& frame)
{
    auto& pause = *roster_pause;
    pending_tics = 0;
    if (std::chrono::steady_clock::now() - pause.started_at > std::chrono::seconds(15)) {
        failure = "Roster change stalled. Simulation paused. Press Escape to close.";
        return;
    }
    waiting = "Pausing for roster change.";
    if (!pause.stop_sent) {
        svanes::MessageWriter writer;
        writer.WriteUint64(pause.local_stop.tick);
        writer.WriteBool(pause.local_stop.leaving);
        writer.WriteUint32(static_cast<std::uint32_t>(pause.local_stop.joining.size()));
        for (const auto& address : pause.local_stop.joining) {
            svanes::WriteAddress(writer, address);
        }
        if (!network.Broadcast(GooseMessageType::RosterStop, writer.Finish())) {
            return;
        }
        pause.stop_sent = true;
        pause.stops.emplace(network.LocalPeer().value, pause.local_stop);
    }
    if (pause.stops.size() != players.size()) {
        return;
    }
    std::uint64_t boundary = 0;
    std::vector<svanes::PeerId> departing;
    std::vector<std::pair<std::uint32_t, svanes::UdpAddress>> joining;
    svanes::MessageWriter roster_writer;
    roster_writer.WriteUint64(network.Revision());
    roster_writer.WriteUint32(next_peer_id);
    for (const auto& [id, stop] : pause.stops) {
        boundary = std::max(boundary, stop.tick);
        if (stop.leaving) {
            departing.push_back({id});
        }
        roster_writer.WriteUint32(id);
        roster_writer.WriteUint64(stop.tick);
        roster_writer.WriteBool(stop.leaving);
        roster_writer.WriteUint32(static_cast<std::uint32_t>(stop.joining.size()));
        for (const auto& address : stop.joining) {
            svanes::WriteAddress(roster_writer, address);
            joining.emplace_back(id, address);
        }
    }
    if (departing.empty() && joining.empty()) {
        failure = "Roster pause has no roster changes.";
        return;
    }
    const auto roster_hash = svanes::HashBytes(roster_writer.Finish().bytes);
    waiting = "Finishing inputs through roster boundary " + std::to_string(boundary) + ".";
    for (std::uint32_t step = 0; simulation.Tick() < boundary && step < StepsPerFrame; ++step) {
        const GooseIntent neutral{};
        if (!network.Broadcast(GooseMessageType::Input, EncodeInput(simulation.Tick(), neutral))) {
            return;
        }
        RecordInput(local_index, simulation.Tick(), neutral);
        AdvanceTick(frame);
    }
    if (simulation.Tick() != boundary || ConfirmedTick() != boundary) {
        return;
    }
    waiting = "Verifying world and departures at tick " + std::to_string(boundary) + ".";
    const PreparedRecord local{boundary, GooseSimulation::Hash(simulation.Capture(frame.world)), roster_hash};
    if (!pause.prepared.contains(network.LocalPeer().value)) {
        svanes::MessageWriter writer;
        writer.WriteUint64(local.tick);
        writer.WriteUint64(local.world_hash);
        writer.WriteUint64(local.roster_hash);
        if (!network.Broadcast(GooseMessageType::RosterPrepared, writer.Finish())) {
            return;
        }
        pause.prepared.emplace(network.LocalPeer().value, local);
    }
    for (const auto& [id, prepared] : pause.prepared) {
        if (prepared != local) {
            failure = "Roster state mismatch with peer " + std::to_string(id) +
                " at tick " + std::to_string(boundary) + ". Simulation paused.";
            return;
        }
    }
    if (pause.prepared.size() != players.size()) {
        return;
    }
    departed = pause.local_stop.leaving;
    if (departed) {
        network.ApplyRosterChange(departing, {});
        roster_pause.reset();
        return;
    }
    for (const auto peer : departing) {
        simulation.RemovePlayer(frame.world, peer);
    }
    const auto remaining = players.size() - departing.size();
    const auto open_slots = GooseMaxPlayers > remaining ? GooseMaxPlayers - remaining : 0;
    std::vector<GoosePeerEndpoint> admitted;
    std::vector<std::pair<JoinCandidate, std::optional<svanes::PeerId>>> answers;
    for (std::size_t index = 0; index < joining.size(); ++index) {
        std::optional<svanes::PeerId> id;
        if (index < open_slots) {
            if (next_peer_id >= GooseUnassignedPeer.value) {
                throw std::overflow_error("Goose peer ids exhausted.");
            }
            id = svanes::PeerId{next_peer_id++};
            simulation.AddPlayer(frame.world, *id);
            const auto sponsor = joining[index].first;
            admitted.push_back({*id, sponsor == network.LocalPeer().value
                ? joining[index].second
                : ResolveRelayedAddress(joining[index].second, network.PeerAddress({sponsor}).host)});
        }
        if (joining[index].first == network.LocalPeer().value) {
            const auto candidate = std::find_if(pause.sponsored.begin(), pause.sponsored.end(),
                [&](const auto& sponsored) { return sponsored.address == joining[index].second; });
            answers.emplace_back(*candidate, id);
        }
    }
    network.ApplyRosterChange(departing, admitted);
    ResetPlayers();
    for (const auto& [candidate, id] : answers) {
        if (!id) {
            RejectJoin(candidate.connection, "the game is full");
            continue;
        }
        GooseAssignment assignment{*id, network.LocalPeer(), network.Revision(), {}};
        for (const auto& endpoint : network.RemoteEndpoints()) {
            if (endpoint.peer != *id) {
                assignment.peers.push_back(endpoint);
            }
        }
        const auto payload = EncodeAssignment(assignment);
        assignments.insert_or_assign(candidate.connection.value,
            SentAssignment{payload, std::chrono::steady_clock::now()});
        network.SendStranger(candidate.connection, GooseMessageType::JoinAssigned, payload);
        network.SendSnapshot(*id, EncodeJoinSnapshot(frame.world));
    }
    roster_pause.reset();
    controls = {};
    started = false;
    waiting = "Roster change complete. " + std::to_string(players.size()) + " player(s) in the world.";
}

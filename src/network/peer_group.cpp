#include <svanes/network/peer_group.hpp>

#include "peer_join.hpp"

#include <svanes/network/message_serialization.hpp>
#include <svanes/utility/hash.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace svanes {

namespace {

constexpr std::size_t kSnapshotChunkBytes = 1024;
constexpr std::uint32_t kMaximumSnapshotChunks = 4096;
constexpr std::size_t kMaximumQueuedMessages = 4096;
constexpr auto kAssignmentRetention = std::chrono::seconds(15);
constexpr auto kRosterChangeTimeout = std::chrono::seconds(15);
constexpr auto kDeliveryTimeout = std::chrono::milliseconds(2000);

/**
 * @return Session settings that report an unresponsive member after two
 * seconds.
 */
ReliabilitySettings PeerReliability() {
    ReliabilitySettings reliability;
    reliability.delivery_timeout = kDeliveryTimeout;
    return reliability;
}

/**
 * @param ids Sorted or unsorted ids.
 * @param id The id to look for.
 * @return Whether the id is present.
 */
bool Contains(const std::vector<std::uint32_t> &ids, std::uint32_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

/**
 * @param peers The roster to sort.
 */
void SortPeers(std::vector<PeerId> &peers) {
    std::sort(peers.begin(), peers.end(),
              [](PeerId a, PeerId b) { return a.value < b.value; });
}

/**
 * @param type A message type.
 * @return Whether PeerGroup sends that type for itself.
 */
bool IsPeerGroupType(MessageType type) {
    return type >= static_cast<MessageType>(PeerMessageType::SnapshotChunk) &&
           type <= static_cast<MessageType>(PeerMessageType::RosterPrepared);
}

/**
 * @param type A message type.
 * @return Whether the message belongs to the roster change agreement.
 */
bool IsRosterType(MessageType type) {
    return type == static_cast<MessageType>(PeerMessageType::RosterStop) ||
           type == static_cast<MessageType>(PeerMessageType::RosterPrepared);
}

} // namespace

NetworkMessage EncodeAssignment(const PeerAssignment &assignment) {
    MessageWriter writer;
    writer.WriteUint32(assignment.local_peer.value);
    writer.WriteUint32(assignment.sponsor.value);
    writer.WriteUint64(assignment.revision);
    writer.WriteUint32(static_cast<std::uint32_t>(assignment.peers.size()));
    for (const auto &endpoint : assignment.peers) {
        writer.WriteUint32(endpoint.peer.value);
        WriteAddress(writer, endpoint.address);
    }
    return writer.Finish();
}

PeerAssignment DecodeAssignment(const NetworkMessage &payload) {
    MessageReader reader(payload);
    PeerAssignment assignment;
    assignment.local_peer = PeerId{reader.ReadUint32()};
    assignment.sponsor = PeerId{reader.ReadUint32()};
    assignment.revision = reader.ReadUint64();
    const auto count = reader.ReadUint32();
    for (std::uint32_t index = 0; index < count; ++index) {
        const PeerId peer{reader.ReadUint32()};
        assignment.peers.push_back({peer, ReadAddress(reader)});
    }
    if (reader.Remaining() != 0 || assignment.local_peer.value == 0 ||
        assignment.sponsor.value == 0 || assignment.revision == 0) {
        throw std::invalid_argument(
            "Peer assignment has invalid ids, revision, or length.");
    }
    return assignment;
}

UdpAddress ResolveRelayedAddress(const UdpAddress &relayed,
                                 const std::string &relay_host) {
    if (relayed.host.starts_with("127.")) {
        return {relay_host, relayed.port};
    }
    return relayed;
}

PeerGroup::PeerGroup(std::unique_ptr<UdpMsgPipe> bound_pipe,
                     PeerSettings settings)
    : settings(settings), pipe(bound_pipe.get()), peers{{1}},
      world_loaded(true) {
    session = std::make_unique<NetworkSession>(
        std::move(bound_pipe), SessionConfiguration{settings.session, {1}, {}},
        PeerReliability());
}

PeerGroup::PeerGroup(std::unique_ptr<UdpMsgPipe> bound_pipe, UdpAddress entry,
                     PeerSettings settings)
    : settings(settings), pipe(bound_pipe.get()) {
    session = std::make_unique<NetworkSession>(
        std::move(bound_pipe), SessionConfiguration{settings.session, {}, {}},
        PeerReliability());
    join = std::make_unique<internal::PeerJoin>(*session, *pipe,
                                                std::move(entry), RulesHash());
}

PeerGroup::~PeerGroup() = default;

void PeerGroup::Attach(RosterParticipant &roster_participant) {
    participant = &roster_participant;
}

std::uint64_t PeerGroup::RulesHash() const {
    MessageWriter writer;
    writer.WriteUint64(settings.rules_hash);
    writer.WriteUint32(settings.max_players);
    return HashBytes(writer.Finish().bytes);
}

void PeerGroup::Adopt(ConnectionId sponsor_connection,
                      const PeerAssignment &assignment) {
    session->Admit(assignment.local_peer);
    revision = assignment.revision;
    session->AddPeer({assignment.sponsor, sponsor_connection});
    peers = {assignment.local_peer, assignment.sponsor};
    const auto sponsor_host = pipe->RemoteAddress(sponsor_connection).host;
    for (const auto &endpoint : assignment.peers) {
        const auto address =
            ResolveRelayedAddress(endpoint.address, sponsor_host);
        session->AddPeer(
            {endpoint.peer, pipe->AddRemote(address.host, address.port)});
        peers.push_back(endpoint.peer);
    }
    SortPeers(peers);
}

void PeerGroup::Update() {
    if (!participant) {
        throw std::logic_error(
            "PeerGroup::Update requires an attached RosterParticipant.");
    }
    session->Update();
    if (join) {
        join->Update();
        if (!join->Failure().empty()) {
            failure = join->Failure();
        } else if (join->Assignment()) {
            Adopt(join->EntryConnection(), *join->Assignment());
            join.reset();
        }
        return;
    }
    PeerId failed_peer;
    while (session->ReceivePeerFailure(failed_peer)) {
        if (!world_loaded) {
            failure = "Peer " + std::to_string(failed_peer.value) +
                      " stopped responding before the world arrived.";
        } else if (!Contains(unreachable, failed_peer.value)) {
            unreachable.push_back(failed_peer.value);
            std::sort(unreachable.begin(), unreachable.end());
            Discard(failed_peer);
        }
    }
    if (HasFailed() || departed) {
        messages.clear();
        return;
    }

    SessionMessage message;
    while (session->Receive(message)) {
        if (message.type ==
            static_cast<MessageType>(PeerMessageType::SnapshotChunk)) {
            ReadSnapshotChunk(message);
        } else {
            QueueTaggedMessage(std::move(message));
            if (HasFailed()) {
                return;
            }
        }
    }

    if (!world_loaded) {
        if (!snapshot_chunk_count ||
            snapshot_chunks.size() != *snapshot_chunk_count) {
            return;
        }
        NetworkMessage snapshot;
        for (const auto &[index, bytes] : snapshot_chunks) {
            snapshot.bytes.insert(snapshot.bytes.end(), bytes.begin(),
                                  bytes.end());
        }
        snapshot_chunks.clear();
        LoadJoinSnapshot(snapshot);
        world_loaded = true;
    }

    for (auto &outgoing : outgoing_snapshots) {
        while (outgoing.next < outgoing.chunks.size() &&
               session->Send(
                   outgoing.peer,
                   static_cast<MessageType>(PeerMessageType::SnapshotChunk),
                   outgoing.chunks[outgoing.next])) {
            ++outgoing.next;
        }
    }
    std::erase_if(outgoing_snapshots, [](const auto &outgoing) {
        return outgoing.next == outgoing.chunks.size();
    });

    HandleJoinRequests();
    ProcessRosterMessages();
    if (HasFailed()) {
        return;
    }
    if (!roster_change && (leave_requested || !join_requests.empty() ||
                           !unreachable.empty())) {
        BeginRosterChange();
    }
    if (roster_change) {
        UpdateRosterChange();
    }
}

void PeerGroup::QueueTaggedMessage(SessionMessage message) {
    if (Contains(discarded, message.sender.value)) {
        return;
    }
    MessageReader reader(message.payload);
    const auto message_revision = reader.ReadUint64();
    if (message_revision < revision) {
        return;
    }
    if (message_revision - revision > 1) {
        throw std::invalid_argument(
            "PeerGroup message is beyond the next roster revision.");
    }
    const auto body = reader.ReadBytes(reader.Remaining());
    NetworkMessage payload;
    payload.bytes.assign(body.begin(), body.end());
    message.payload = std::move(payload);
    auto &queue = message_revision == revision ? messages : future_messages;
    if (queue.size() >= kMaximumQueuedMessages) {
        failure = "Incoming message queue exceeded its limit.";
        messages.clear();
        return;
    }
    queue.push_back(std::move(message));
}

void PeerGroup::ReadSnapshotChunk(const SessionMessage &message) {
    MessageReader reader(message.payload);
    const auto index = reader.ReadUint32();
    const auto count = reader.ReadUint32();
    const auto body = reader.ReadBytes(reader.Remaining());
    if (world_loaded || count == 0 || count > kMaximumSnapshotChunks ||
        index >= count ||
        (snapshot_chunk_count && *snapshot_chunk_count != count)) {
        throw std::invalid_argument("Unexpected or inconsistent snapshot "
                                    "chunk.");
    }
    snapshot_chunk_count = count;
    snapshot_chunks.emplace(index,
                            std::vector<std::byte>(body.begin(), body.end()));
}

void PeerGroup::LoadJoinSnapshot(const NetworkMessage &snapshot) {
    MessageReader reader(snapshot);
    const auto next = reader.ReadUint32();
    const auto count = reader.ReadUint32();
    if (count == 0 || count > settings.max_players) {
        throw std::invalid_argument(
            "Joined world snapshot has an invalid member count.");
    }
    std::vector<PeerId> roster;
    for (std::uint32_t index = 0; index < count; ++index) {
        roster.push_back({reader.ReadUint32()});
    }
    const auto hash = reader.ReadUint64();
    const auto world = reader.ReadBytes(reader.Remaining());
    if (roster != peers) {
        throw std::runtime_error("The received world's members differ from "
                                 "the members this process was told about.");
    }
    if (next <= roster.back().value) {
        throw std::invalid_argument(
            "Joined world snapshot has an invalid next id.");
    }
    participant->LoadWorld(roster, world);
    if (HashBytes(participant->SaveWorld().bytes) != hash) {
        throw std::runtime_error(
            "The rebuilt world does not match the sponsor's world hash.");
    }
    next_peer_id = next;
}

void PeerGroup::SendSnapshot(PeerId peer, const NetworkMessage &snapshot) {
    const auto count = (snapshot.bytes.size() + kSnapshotChunkBytes - 1) /
                       kSnapshotChunkBytes;
    if (count == 0 || count > kMaximumSnapshotChunks) {
        throw std::length_error("World snapshot is empty or too large to "
                                "send.");
    }
    OutgoingSnapshot outgoing{peer, {}, 0};
    for (std::size_t index = 0; index < count; ++index) {
        const auto begin = index * kSnapshotChunkBytes;
        const auto length =
            std::min(kSnapshotChunkBytes, snapshot.bytes.size() - begin);
        MessageWriter writer;
        writer.WriteUint32(static_cast<std::uint32_t>(index));
        writer.WriteUint32(static_cast<std::uint32_t>(count));
        writer.WriteBytes(std::span{snapshot.bytes}.subspan(begin, length));
        outgoing.chunks.push_back(writer.Finish());
    }
    outgoing_snapshots.push_back(std::move(outgoing));
}

bool PeerGroup::Broadcast(MessageType type, const NetworkMessage &payload) {
    if (IsPeerGroupType(type)) {
        throw std::invalid_argument(
            "PeerGroup::Broadcast cannot send PeerGroup's own message types.");
    }
    return BroadcastTagged(type, payload);
}

bool PeerGroup::BroadcastTagged(MessageType type,
                                const NetworkMessage &payload) {
    MessageWriter writer;
    writer.WriteUint64(revision);
    writer.WriteBytes(payload.bytes);
    return !departed && !HasFailed() &&
           session->Broadcast(type, writer.Finish());
}

bool PeerGroup::Receive(SessionMessage &message) {
    if (HasFailed() || messages.empty()) {
        return false;
    }
    message = std::move(messages.front());
    messages.pop_front();
    return true;
}

void PeerGroup::HandleJoinRequests() {
    const auto now = std::chrono::steady_clock::now();
    std::erase_if(assignments, [&](const auto &entry) {
        return now - entry.second.sent_at > kAssignmentRetention ||
               std::find(peers.begin(), peers.end(), entry.second.peer) ==
                   peers.end();
    });
    StrangerMessage stranger;
    while (session->ReceiveStranger(stranger)) {
        if (!stranger.contact ||
            stranger.message.type !=
                static_cast<MessageType>(PeerMessageType::JoinRequest)) {
            continue;
        }
        const auto connection = stranger.connection;
        const auto assigned = assignments.find(connection.value);
        if (assigned != assignments.end()) {
            static_cast<void>(session->SendStranger(
                connection,
                static_cast<MessageType>(PeerMessageType::JoinAssigned),
                assigned->second.payload));
            continue;
        }
        const auto same = [&](const JoinCandidate &candidate) {
            return candidate.connection == connection;
        };
        const auto sponsored =
            roster_change ? roster_change->sponsored.size() : 0;
        if (std::any_of(join_requests.begin(), join_requests.end(), same) ||
            (roster_change &&
             std::any_of(roster_change->sponsored.begin(),
                         roster_change->sponsored.end(), same))) {
            static_cast<void>(session->SendStranger(
                connection,
                static_cast<MessageType>(PeerMessageType::JoinPending), {}));
            continue;
        }
        if (stranger.message.payload.bytes.size() != sizeof(std::uint64_t)) {
            RejectJoin(connection, "malformed join request");
            continue;
        }
        MessageReader reader(stranger.message.payload);
        if (reader.ReadUint64() != RulesHash()) {
            RejectJoin(connection, "incompatible game version");
        } else if (leave_requested || departed) {
            RejectJoin(connection, "the player you contacted is leaving");
        } else if (session->ActivePeerAt(connection)) {
            RejectJoin(connection, "that address is already in the game");
        } else if (peers.size() + join_requests.size() + sponsored >=
                   settings.max_players) {
            RejectJoin(connection, "the game is full");
        } else {
            join_requests.push_back(
                {connection, pipe->RemoteAddress(connection)});
            static_cast<void>(session->SendStranger(
                connection,
                static_cast<MessageType>(PeerMessageType::JoinPending), {}));
        }
    }
}

void PeerGroup::RejectJoin(ConnectionId connection, std::string_view reason) {
    MessageWriter writer;
    writer.WriteText(reason);
    static_cast<void>(session->SendStranger(
        connection, static_cast<MessageType>(PeerMessageType::JoinRejected),
        writer.Finish()));
}

void PeerGroup::ProcessRosterMessages() {
    std::deque<SessionMessage> kept;
    for (auto &message : messages) {
        if (!IsRosterType(message.type)) {
            kept.push_back(std::move(message));
            continue;
        }
        ReceiveRosterMessage(message);
        if (HasFailed()) {
            return;
        }
    }
    messages = std::move(kept);
}

void PeerGroup::ReceiveRosterMessage(const SessionMessage &message) {
    if (message.sender == LocalPeer() ||
        std::find(peers.begin(), peers.end(), message.sender) == peers.end()) {
        throw std::invalid_argument("Roster message has an invalid sender.");
    }
    BeginRosterChange();
    MessageReader reader(message.payload);
    if (message.type == static_cast<MessageType>(PeerMessageType::RosterStop)) {
        const auto record = ReadStop(reader, settings.max_players);
        const auto [entry, inserted] =
            roster_change->stops.emplace(message.sender.value, record);
        if (!inserted && entry->second != record) {
            failure = "Peer changed its roster stop announcement.";
        }
    } else {
        const auto boundary = reader.ReadUint64();
        const auto world_hash = reader.ReadUint64();
        const PreparedRecord record{boundary, world_hash, reader.ReadUint64()};
        const auto [entry, inserted] =
            roster_change->prepared.emplace(message.sender.value, record);
        if (!inserted && entry->second != record) {
            failure = "Peer changed its prepared roster state.";
        }
    }
    if (reader.Remaining() != 0) {
        throw std::invalid_argument("Roster message has trailing data.");
    }
}

void PeerGroup::BeginRosterChange() {
    if (roster_change) {
        return;
    }
    RosterChange change;
    change.local_stop.position = participant->Freeze();
    change.local_stop.leaving = leave_requested;
    change.local_stop.dropped = unreachable;
    for (const auto peer : peers) {
        if (peer != LocalPeer()) {
            change.local_stop.progress.emplace(peer.value,
                                               participant->ProgressOf(peer));
        }
    }
    change.started_at = std::chrono::steady_clock::now();
    if (leave_requested) {
        for (const auto &candidate : join_requests) {
            RejectJoin(candidate.connection,
                       "the player you contacted is leaving");
        }
    } else {
        change.sponsored = std::move(join_requests);
        for (const auto &candidate : change.sponsored) {
            change.local_stop.joining.push_back(candidate.address);
        }
    }
    join_requests.clear();
    roster_change = std::move(change);
    status = unreachable.empty()
                 ? "Pausing for roster change."
                 : "Peer " + std::to_string(unreachable.front()) +
                       " stopped responding. Removing it from the world.";
}

void PeerGroup::WriteStop(MessageWriter &writer, const StopRecord &stop) {
    writer.WriteUint64(stop.position);
    writer.WriteBool(stop.leaving);
    writer.WriteUint32(static_cast<std::uint32_t>(stop.joining.size()));
    for (const auto &address : stop.joining) {
        WriteAddress(writer, address);
    }
    writer.WriteUint32(static_cast<std::uint32_t>(stop.dropped.size()));
    for (const auto id : stop.dropped) {
        writer.WriteUint32(id);
    }
    writer.WriteUint32(static_cast<std::uint32_t>(stop.progress.size()));
    for (const auto &[id, position] : stop.progress) {
        writer.WriteUint32(id);
        writer.WriteUint64(position);
    }
}

PeerGroup::StopRecord PeerGroup::ReadStop(MessageReader &reader,
                                          std::uint32_t max_players) {
    StopRecord stop;
    stop.position = reader.ReadUint64();
    stop.leaving = reader.ReadBool();
    const auto joining = reader.ReadUint32();
    if (joining > max_players) {
        throw std::invalid_argument("Roster stop admits too many players.");
    }
    for (std::uint32_t index = 0; index < joining; ++index) {
        stop.joining.push_back(ReadAddress(reader));
    }
    const auto dropped = reader.ReadUint32();
    if (dropped > max_players) {
        throw std::invalid_argument("Roster stop drops too many players.");
    }
    for (std::uint32_t index = 0; index < dropped; ++index) {
        stop.dropped.push_back(reader.ReadUint32());
    }
    const auto progress = reader.ReadUint32();
    if (progress > max_players) {
        throw std::invalid_argument("Roster stop reports too many players.");
    }
    for (std::uint32_t index = 0; index < progress; ++index) {
        const auto id = reader.ReadUint32();
        stop.progress.emplace(id, reader.ReadUint64());
    }
    return stop;
}

std::vector<std::uint32_t> PeerGroup::DroppedMembers() const {
    std::vector<std::uint32_t> dropped = roster_change->local_stop.dropped;
    for (const auto &[id, stop] : roster_change->stops) {
        for (const auto peer : stop.dropped) {
            if (!Contains(dropped, peer)) {
                dropped.push_back(peer);
            }
        }
    }
    std::sort(dropped.begin(), dropped.end());
    return dropped;
}

void PeerGroup::Discard(PeerId peer) {
    if (Contains(discarded, peer.value)) {
        return;
    }
    discarded.push_back(peer.value);
    session->RetirePeer(peer);
    const auto from_peer = [&](const SessionMessage &message) {
        return message.sender == peer;
    };
    std::erase_if(messages, from_peer);
    std::erase_if(future_messages, from_peer);
}

void PeerGroup::UpdateRosterChange() {
    auto &change = *roster_change;
    if (std::chrono::steady_clock::now() - change.started_at >
        kRosterChangeTimeout) {
        failure = "Roster change stalled.";
        return;
    }
    if (!change.stop_sent) {
        MessageWriter writer;
        WriteStop(writer, change.local_stop);
        if (!BroadcastTagged(
                static_cast<MessageType>(PeerMessageType::RosterStop),
                writer.Finish())) {
            return;
        }
        change.stop_sent = true;
        change.stops.emplace(LocalPeer().value, change.local_stop);
    }
    const auto dropped = DroppedMembers();
    for (const auto id : dropped) {
        if (id != LocalPeer().value) {
            Discard({id});
        }
    }
    if (Contains(dropped, LocalPeer().value)) {
        removed = true;
        departed = true;
        ApplyRosterChange(std::vector<PeerId>{LocalPeer()}, {});
        roster_change.reset();
        return;
    }
    std::vector<std::uint32_t> survivors;
    for (const auto peer : peers) {
        if (!Contains(dropped, peer.value)) {
            if (!change.stops.contains(peer.value)) {
                return;
            }
            survivors.push_back(peer.value);
        }
    }

    std::uint64_t boundary = 0;
    std::vector<PeerId> departing;
    std::vector<std::pair<std::uint32_t, UdpAddress>> joining;
    MessageWriter roster_writer;
    roster_writer.WriteUint64(revision);
    roster_writer.WriteUint32(next_peer_id);
    roster_writer.WriteUint32(static_cast<std::uint32_t>(dropped.size()));
    for (const auto id : dropped) {
        roster_writer.WriteUint32(id);
        departing.push_back({id});
    }
    for (const auto id : survivors) {
        const auto &stop = change.stops.at(id);
        boundary = std::max(boundary, stop.position);
        if (stop.leaving) {
            departing.push_back({id});
        }
        roster_writer.WriteUint32(id);
        WriteStop(roster_writer, stop);
        for (const auto &address : stop.joining) {
            joining.emplace_back(id, address);
        }
    }
    if (departing.empty() && joining.empty()) {
        failure = "Roster change has no departures or joins.";
        return;
    }
    const auto roster_hash = HashBytes(roster_writer.Finish().bytes);
    if (!change.abandoned) {
        for (const auto id : dropped) {
            auto cutoff = std::numeric_limits<std::uint64_t>::max();
            for (const auto survivor : survivors) {
                const auto &progress = change.stops.at(survivor).progress;
                const auto found = progress.find(id);
                if (found == progress.end()) {
                    throw std::invalid_argument(
                        "Roster stop lacks progress for a dropped member.");
                }
                cutoff = std::min(cutoff, found->second);
            }
            participant->Abandon({id}, cutoff, boundary);
        }
        change.abandoned = true;
    }
    status = "Finishing up to roster boundary " + std::to_string(boundary) + ".";
    if (!participant->SettleAt(boundary)) {
        return;
    }

    status = "Verifying world and roster at boundary " +
             std::to_string(boundary) + ".";
    const PreparedRecord local{
        boundary, HashBytes(participant->SaveWorld().bytes), roster_hash};
    if (!change.prepared.contains(LocalPeer().value)) {
        MessageWriter writer;
        writer.WriteUint64(local.boundary);
        writer.WriteUint64(local.world_hash);
        writer.WriteUint64(local.roster_hash);
        if (!BroadcastTagged(
                static_cast<MessageType>(PeerMessageType::RosterPrepared),
                writer.Finish())) {
            return;
        }
        change.prepared.emplace(LocalPeer().value, local);
    }
    for (const auto &[id, prepared] : change.prepared) {
        if (Contains(dropped, id)) {
            continue;
        }
        if (prepared != local) {
            failure = "Roster state mismatch with peer " + std::to_string(id) +
                      " at boundary " + std::to_string(boundary) +
                      ". Simulation paused.";
            return;
        }
    }
    for (const auto id : survivors) {
        if (!change.prepared.contains(id)) {
            return;
        }
    }

    if (change.local_stop.leaving) {
        departed = true;
        ApplyRosterChange(departing, {});
        roster_change.reset();
        return;
    }
    const auto remaining = peers.size() - departing.size();
    const auto open_slots =
        settings.max_players > remaining ? settings.max_players - remaining : 0;
    std::vector<PeerEndpoint> admitted;
    std::vector<PeerId> joined;
    std::vector<std::pair<JoinCandidate, std::optional<PeerId>>> answers;
    for (std::size_t index = 0; index < joining.size(); ++index) {
        const auto [sponsor, address] = joining[index];
        std::optional<PeerId> id;
        if (index < open_slots) {
            if (next_peer_id == std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("PeerGroup peer ids exhausted.");
            }
            id = PeerId{next_peer_id++};
            joined.push_back(*id);
            admitted.push_back(
                {*id, sponsor == LocalPeer().value
                          ? address
                          : ResolveRelayedAddress(
                                address, PeerAddress({sponsor}).host)});
        }
        if (sponsor == LocalPeer().value) {
            const auto candidate = std::find_if(
                change.sponsored.begin(), change.sponsored.end(),
                [&](const auto &sponsored) {
                    return sponsored.address == address;
                });
            answers.emplace_back(*candidate, id);
        }
    }
    ApplyRosterChange(departing, admitted);
    participant->ChangeRoster(departing, joined);
    std::erase_if(unreachable,
                  [&](std::uint32_t id) { return Contains(dropped, id); });
    std::erase_if(discarded,
                  [&](std::uint32_t id) { return Contains(dropped, id); });

    std::optional<NetworkMessage> join_snapshot;
    for (const auto &[candidate, id] : answers) {
        if (!id) {
            RejectJoin(candidate.connection, "the game is full");
            continue;
        }
        PeerAssignment assignment{*id, LocalPeer(), revision, {}};
        for (const auto &endpoint : RemoteEndpoints()) {
            if (endpoint.peer != *id) {
                assignment.peers.push_back(endpoint);
            }
        }
        const auto payload = EncodeAssignment(assignment);
        assignments.insert_or_assign(
            candidate.connection.value,
            SentAssignment{*id, payload, std::chrono::steady_clock::now()});
        static_cast<void>(session->SendStranger(
            candidate.connection,
            static_cast<MessageType>(PeerMessageType::JoinAssigned), payload));
        if (!join_snapshot) {
            const auto world = participant->SaveWorld();
            MessageWriter writer;
            writer.WriteUint32(next_peer_id);
            writer.WriteUint32(static_cast<std::uint32_t>(peers.size()));
            for (const auto peer : peers) {
                writer.WriteUint32(peer.value);
            }
            writer.WriteUint64(HashBytes(world.bytes));
            writer.WriteBytes(world.bytes);
            join_snapshot = writer.Finish();
        }
        SendSnapshot(*id, *join_snapshot);
    }
    roster_change.reset();
    ProcessRosterMessages();
}

void PeerGroup::ApplyRosterChange(std::span<const PeerId> departing,
                                  std::span<const PeerEndpoint> joining) {
    if (revision == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("PeerGroup roster revision exhausted.");
    }
    const bool local_departing = std::find(departing.begin(), departing.end(),
                                           LocalPeer()) != departing.end();
    std::vector<PeerId> retiring;
    for (const auto &remote : session->RemotePeers()) {
        if (local_departing || std::find(departing.begin(), departing.end(),
                                         remote.peer) != departing.end()) {
            retiring.push_back(remote.peer);
        }
    }
    for (const auto peer : retiring) {
        session->RetirePeer(peer);
    }
    std::erase_if(peers, [&](PeerId peer) {
        return std::find(departing.begin(), departing.end(), peer) !=
               departing.end();
    });
    if (!local_departing) {
        for (const auto &endpoint : joining) {
            session->AddPeer(
                {endpoint.peer, pipe->AddRemote(endpoint.address.host,
                                                endpoint.address.port)});
            peers.push_back(endpoint.peer);
        }
        SortPeers(peers);
    }
    ++revision;
    messages = std::move(future_messages);
    future_messages.clear();
}

void PeerGroup::RequestLeave() {
    if (HasFailed() || !world_loaded) {
        force_close = true;
    } else {
        leave_requested = true;
    }
}

bool PeerGroup::HasDeparted() const { return departed; }

bool PeerGroup::CanClose() const {
    return force_close || (departed && session->OutgoingDrained());
}

bool PeerGroup::IsRunning() const { return world_loaded && !departed; }

bool PeerGroup::IsChangingRoster() const { return roster_change.has_value(); }

UdpAddress PeerGroup::PeerAddress(PeerId peer) const {
    for (const auto &endpoint : RemoteEndpoints()) {
        if (endpoint.peer == peer) {
            return endpoint.address;
        }
    }
    throw std::invalid_argument(
        "PeerGroup::PeerAddress: peer is not a remote member.");
}

std::vector<PeerEndpoint> PeerGroup::RemoteEndpoints() const {
    std::vector<PeerEndpoint> endpoints;
    for (const auto &remote : session->RemotePeers()) {
        if (std::find(peers.begin(), peers.end(), remote.peer) != peers.end()) {
            endpoints.push_back(
                {remote.peer, pipe->RemoteAddress(remote.connection)});
        }
    }
    return endpoints;
}

std::span<const PeerId> PeerGroup::Peers() const { return peers; }

PeerId PeerGroup::LocalPeer() const { return session->LocalPeer(); }

std::uint16_t PeerGroup::Port() const { return pipe->LocalPort(); }

bool PeerGroup::HasFailed() const { return !failure.empty(); }

std::string PeerGroup::Status() const {
    if (HasFailed()) {
        return failure;
    }
    if (join) {
        return join->Status();
    }
    if (!world_loaded) {
        return "Receiving the world.";
    }
    if (removed) {
        return "Removed from the world: the other players stopped hearing "
               "from this game.";
    }
    if (departed) {
        return "Departure agreed. Finishing final message delivery.";
    }
    if (roster_change) {
        return status;
    }
    return "Connected.";
}

std::uint64_t PeerGroup::Revision() const { return revision; }

} // namespace svanes

#include "goose_network.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::size_t kSnapshotChunkBytes = 1024;
constexpr std::uint32_t kMaximumSnapshotChunks = 4096;
constexpr std::size_t kMaximumQueuedMessages = 4096;
constexpr auto kJoinRequestInterval = std::chrono::milliseconds(250);
constexpr auto kJoinTimeout = std::chrono::seconds(15);

}

svanes::UdpAddress ResolveRelayedAddress(const svanes::UdpAddress& relayed, const std::string& relay_host)
{
    if (relayed.host.starts_with("127.")) {
        return {relay_host, relayed.port};
    }
    return relayed;
}

svanes::NetworkMessage EncodeAssignment(const GooseAssignment& assignment)
{
    svanes::MessageWriter writer;
    writer.WriteUint32(assignment.local_peer.value);
    writer.WriteUint32(assignment.sponsor.value);
    writer.WriteUint64(assignment.revision);
    writer.WriteUint32(static_cast<std::uint32_t>(assignment.peers.size()));
    for (const auto& endpoint : assignment.peers) {
        writer.WriteUint32(endpoint.peer.value);
        svanes::WriteAddress(writer, endpoint.address);
    }
    return writer.Finish();
}

GooseAssignment DecodeAssignment(const svanes::NetworkMessage& payload)
{
    svanes::MessageReader reader(payload);
    GooseAssignment assignment{{reader.ReadUint32()}, {reader.ReadUint32()}, reader.ReadUint64(), {}};
    const auto count = reader.ReadUint32();
    for (std::uint32_t index = 0; index < count; ++index) {
        const svanes::PeerId peer{reader.ReadUint32()};
        assignment.peers.push_back({peer, svanes::ReadAddress(reader)});
    }
    if (reader.Remaining() != 0 || assignment.local_peer.value == 0 ||
        assignment.sponsor.value == 0 || assignment.revision == 0) {
        throw std::invalid_argument("Goose assignment has invalid ids, revision, or length.");
    }
    return assignment;
}

GooseNetwork::GooseNetwork(std::unique_ptr<svanes::UdpMsgPipe> bound_pipe)
    : pipe(bound_pipe.get()), peers{{1}}
{
    session = std::make_unique<svanes::NetworkSession>(std::move(bound_pipe),
        svanes::SessionConfiguration{GooseSession, {1}, {}});
}

GooseNetwork::GooseNetwork(std::unique_ptr<svanes::NetworkSession> joined_session, svanes::UdpMsgPipe& joined_pipe,
                           svanes::ConnectionId sponsor_connection, const GooseAssignment& assignment)
    : session(std::move(joined_session)), pipe(&joined_pipe), revision(assignment.revision), awaiting_snapshot(true)
{
    if (session->LocalPeer() != assignment.local_peer) {
        throw std::logic_error("GooseNetwork requires a session admitted under the assigned id.");
    }
    session->AddPeer({assignment.sponsor, sponsor_connection});
    peers = {assignment.local_peer, assignment.sponsor};
    const auto sponsor_host = pipe->RemoteAddress(sponsor_connection).host;
    for (const auto& endpoint : assignment.peers) {
        const auto address = ResolveRelayedAddress(endpoint.address, sponsor_host);
        session->AddPeer({endpoint.peer, pipe->AddRemote(address.host, address.port)});
        peers.push_back(endpoint.peer);
    }
    std::sort(peers.begin(), peers.end(), [](auto a, auto b) { return a.value < b.value; });
}

void GooseNetwork::Update()
{
    session->Update();
    svanes::PeerId failed_peer;
    while (session->ReceivePeerFailure(failed_peer)) {
        if (failure.empty()) {
            failure = "Peer " + std::to_string(failed_peer.value) + " stopped acknowledging messages. Restart the game.";
        }
    }
    if (HasFailed() || local_departed) {
        messages.clear();
        return;
    }

    svanes::SessionMessage message;
    while (session->Receive(message)) {
        switch (static_cast<GooseMessageType>(message.type)) {
        case GooseMessageType::Input:
        case GooseMessageType::StateHash:
        case GooseMessageType::RosterStop:
        case GooseMessageType::RosterPrepared: {
            svanes::MessageReader reader(message.payload);
            const auto message_revision = reader.ReadUint64();
            if (message_revision < revision) {
                break;
            }
            if (message_revision - revision > 1) {
                throw std::invalid_argument("Goose message is beyond the next roster revision.");
            }
            const auto body = reader.ReadBytes(reader.Remaining());
            svanes::NetworkMessage payload;
            payload.bytes.assign(body.begin(), body.end());
            message.payload = std::move(payload);
            auto& queue = message_revision == revision ? messages : future_messages;
            if (queue.size() >= kMaximumQueuedMessages) {
                failure = "Incoming message queue exceeded its limit. Simulation paused.";
                messages.clear();
                return;
            }
            queue.push_back(std::move(message));
            break;
        }
        case GooseMessageType::SnapshotChunk:
            ReadSnapshotChunk(message);
            break;
        default:
            throw std::invalid_argument("GooseNetwork received an unknown game message type.");
        }
    }

    for (auto& outgoing : outgoing_snapshots) {
        while (outgoing.next < outgoing.chunks.size() &&
               session->Send(outgoing.peer, static_cast<svanes::MessageType>(GooseMessageType::SnapshotChunk),
                   outgoing.chunks[outgoing.next])) {
            ++outgoing.next;
        }
    }
    std::erase_if(outgoing_snapshots, [](const auto& outgoing) { return outgoing.next == outgoing.chunks.size(); });
}

void GooseNetwork::ReadSnapshotChunk(const svanes::SessionMessage& message)
{
    svanes::MessageReader reader(message.payload);
    const auto index = reader.ReadUint32();
    const auto count = reader.ReadUint32();
    const auto body = reader.ReadBytes(reader.Remaining());
    if (!awaiting_snapshot || count == 0 || count > kMaximumSnapshotChunks || index >= count ||
        (snapshot_chunk_count && *snapshot_chunk_count != count)) {
        throw std::invalid_argument("Unexpected or inconsistent snapshot chunk.");
    }
    snapshot_chunk_count = count;
    snapshot_chunks.emplace(index, std::vector<std::byte>(body.begin(), body.end()));
}

bool GooseNetwork::Broadcast(GooseMessageType type, const svanes::NetworkMessage& payload)
{
    if (type != GooseMessageType::Input && type != GooseMessageType::StateHash &&
        type != GooseMessageType::RosterStop && type != GooseMessageType::RosterPrepared) {
        throw std::invalid_argument("GooseNetwork::Broadcast requires a gameplay or roster-control type.");
    }
    svanes::MessageWriter writer;
    writer.WriteUint64(revision);
    writer.WriteBytes(payload.bytes);
    return !local_departed && !HasFailed() &&
        session->Broadcast(static_cast<svanes::MessageType>(type), writer.Finish());
}

bool GooseNetwork::Receive(svanes::SessionMessage& message)
{
    if (HasFailed() || messages.empty()) {
        return false;
    }
    message = std::move(messages.front());
    messages.pop_front();
    return true;
}

void GooseNetwork::SendSnapshot(svanes::PeerId peer, const svanes::NetworkMessage& snapshot)
{
    const auto count = (snapshot.bytes.size() + kSnapshotChunkBytes - 1) / kSnapshotChunkBytes;
    if (count == 0 || count > kMaximumSnapshotChunks) {
        throw std::length_error("World snapshot is empty or too large to send.");
    }
    OutgoingSnapshot outgoing{peer, {}, 0};
    for (std::size_t index = 0; index < count; ++index) {
        const auto begin = index * kSnapshotChunkBytes;
        const auto length = std::min(kSnapshotChunkBytes, snapshot.bytes.size() - begin);
        svanes::MessageWriter writer;
        writer.WriteUint32(static_cast<std::uint32_t>(index));
        writer.WriteUint32(static_cast<std::uint32_t>(count));
        writer.WriteBytes(std::span{snapshot.bytes}.subspan(begin, length));
        outgoing.chunks.push_back(writer.Finish());
    }
    outgoing_snapshots.push_back(std::move(outgoing));
}

std::optional<svanes::NetworkMessage> GooseNetwork::TakeSnapshot()
{
    if (!awaiting_snapshot || !snapshot_chunk_count || snapshot_chunks.size() != *snapshot_chunk_count) {
        return std::nullopt;
    }
    svanes::NetworkMessage snapshot;
    for (const auto& [index, bytes] : snapshot_chunks) {
        snapshot.bytes.insert(snapshot.bytes.end(), bytes.begin(), bytes.end());
    }
    awaiting_snapshot = false;
    snapshot_chunks.clear();
    return snapshot;
}

bool GooseNetwork::ReceiveStranger(svanes::StrangerMessage& stranger)
{
    return session->ReceiveStranger(stranger);
}

void GooseNetwork::SendStranger(svanes::ConnectionId connection, GooseMessageType type,
                                const svanes::NetworkMessage& payload)
{
    static_cast<void>(session->SendStranger(connection, static_cast<svanes::MessageType>(type), payload));
}

svanes::UdpAddress GooseNetwork::Address(svanes::ConnectionId connection) const
{
    return pipe->RemoteAddress(connection);
}

std::optional<svanes::PeerId> GooseNetwork::ActivePeerAt(svanes::ConnectionId connection) const
{
    return session->ActivePeerAt(connection);
}

svanes::UdpAddress GooseNetwork::PeerAddress(svanes::PeerId peer) const
{
    for (const auto& endpoint : RemoteEndpoints()) {
        if (endpoint.peer == peer) {
            return endpoint.address;
        }
    }
    throw std::invalid_argument("GooseNetwork::PeerAddress: peer is not a remote member.");
}

std::vector<GoosePeerEndpoint> GooseNetwork::RemoteEndpoints() const
{
    std::vector<GoosePeerEndpoint> endpoints;
    for (const auto& remote : session->RemotePeers()) {
        if (std::find(peers.begin(), peers.end(), remote.peer) != peers.end()) {
            endpoints.push_back({remote.peer, pipe->RemoteAddress(remote.connection)});
        }
    }
    return endpoints;
}

void GooseNetwork::ApplyRosterChange(std::span<const svanes::PeerId> departing,
                                     std::span<const GoosePeerEndpoint> joining)
{
    if (revision == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Goose roster revision exhausted.");
    }
    local_departed = std::find(departing.begin(), departing.end(), LocalPeer()) != departing.end();
    std::vector<svanes::PeerId> retiring;
    for (const auto& remote : session->RemotePeers()) {
        if (local_departed || std::find(departing.begin(), departing.end(), remote.peer) != departing.end()) {
            retiring.push_back(remote.peer);
        }
    }
    for (const auto peer : retiring) {
        session->RetirePeer(peer);
    }
    std::erase_if(peers, [&](auto peer) {
        return std::find(departing.begin(), departing.end(), peer) != departing.end();
    });
    if (!local_departed) {
        for (const auto& endpoint : joining) {
            session->AddPeer({endpoint.peer, pipe->AddRemote(endpoint.address.host, endpoint.address.port)});
            peers.push_back(endpoint.peer);
        }
        std::sort(peers.begin(), peers.end(), [](auto a, auto b) { return a.value < b.value; });
    }
    ++revision;
    messages = std::move(future_messages);
    future_messages.clear();
}

std::span<const svanes::PeerId> GooseNetwork::Peers() const
{
    return peers;
}

svanes::PeerId GooseNetwork::LocalPeer() const
{
    return session->LocalPeer();
}

std::uint16_t GooseNetwork::Port() const
{
    return pipe->LocalPort();
}

bool GooseNetwork::HasFailed() const
{
    return !failure.empty();
}

std::string GooseNetwork::Status() const
{
    return HasFailed() ? failure : "Connected.";
}

bool GooseNetwork::OutgoingDrained() const
{
    return session->OutgoingDrained();
}

std::uint64_t GooseNetwork::Revision() const
{
    return revision;
}

GooseJoin::GooseJoin(svanes::UdpAddress entry, std::uint16_t port, std::uint64_t rules_hash)
    : entry(std::move(entry)), rules_hash(rules_hash), started_at(std::chrono::steady_clock::now())
{
    auto bound_pipe = std::make_unique<svanes::UdpMsgPipe>(port);
    pipe = bound_pipe.get();
    entry_connection = pipe->AddRemote(this->entry.host, this->entry.port);
    session = std::make_unique<svanes::NetworkSession>(std::move(bound_pipe),
        svanes::SessionConfiguration{GooseSession, {}, {}});
}

void GooseJoin::Update()
{
    if (assignment || HasFailed()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - started_at > kJoinTimeout) {
        failure = answered
            ? "Not admitted within 15 seconds."
            : "No reply from " + entry.host + ":" + std::to_string(entry.port) +
                ". Check the address and that the game is running there.";
        return;
    }
    if (!last_request || now - *last_request >= kJoinRequestInterval) {
        svanes::MessageWriter writer;
        writer.WriteUint64(rules_hash);
        static_cast<void>(session->SendStranger(entry_connection,
            static_cast<svanes::MessageType>(GooseMessageType::JoinRequest), writer.Finish()));
        last_request = now;
    }
    session->Update();
    svanes::StrangerMessage stranger;
    while (!assignment && !HasFailed() && session->ReceiveStranger(stranger)) {
        if (stranger.connection != entry_connection || !stranger.contact) {
            continue;
        }
        const auto& packet = stranger.message;
        svanes::MessageReader reader(packet.payload);
        switch (static_cast<GooseMessageType>(packet.type)) {
        case GooseMessageType::JoinPending:
            answered = true;
            break;
        case GooseMessageType::JoinRejected:
            failure = "Join rejected: " + reader.ReadText();
            break;
        case GooseMessageType::JoinAssigned:
            assignment = DecodeAssignment(packet.payload);
            if (assignment->sponsor != packet.sender) {
                throw std::invalid_argument("Goose assignment names a different sponsor than its sender.");
            }
            break;
        default:
            throw std::invalid_argument("GooseJoin received an unexpected contact message.");
        }
    }
}

bool GooseJoin::IsAssigned() const
{
    return assignment.has_value();
}

bool GooseJoin::HasFailed() const
{
    return !failure.empty();
}

std::string GooseJoin::Status() const
{
    if (HasFailed()) {
        return failure;
    }
    if (assignment) {
        return "Admitted as peer " + std::to_string(assignment->local_peer.value) + ". Receiving the world.";
    }
    return answered ? "Waiting to be admitted." : "Contacting " + entry.host + ":" + std::to_string(entry.port) + ".";
}

std::unique_ptr<GooseNetwork> GooseJoin::Admit()
{
    if (!assignment) {
        throw std::logic_error("GooseJoin::Admit called before an assignment arrived.");
    }
    session->Admit(assignment->local_peer);
    return std::make_unique<GooseNetwork>(std::move(session), *pipe, entry_connection, *assignment);
}
